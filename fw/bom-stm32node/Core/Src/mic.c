/**
 ******************************************************************************
 * @file    mic.c
 * @brief   Akvizice z PDM mikrofonu (SAI1_A + GPDMA circular). Z Mik_stm.
 ******************************************************************************
 */
#include "mic.h"
#include "sai.h"          /* extern SAI_HandleTypeDef hsai_BlockA1 (z CubeMX) */

/* --- Kruhovy DMA buffer a DMA linked-list ---------------------------------- */
static uint16_t pdm_ring[PDM_RING_HALFWORDS];   /* kruhovy buffer plneny DMA    */

static DMA_HandleTypeDef hdma_sai_rx;           /* GPDMA1 CH0 <- SAI1_A         */
static DMA_QListTypeDef  pdm_dma_queue;
static DMA_NodeTypeDef   pdm_dma_node;

/* --- Stav akvizice: ISR nastavuje priznaky, main je vycita ----------------- */
static volatile uint8_t  s_running     = 0;
static volatile uint8_t  s_half0_ready = 0;     /* prvni polovina ringu hotova  */
static volatile uint8_t  s_half1_ready = 0;     /* druha polovina ringu hotova  */
static volatile uint8_t  s_overrun     = 0;
static volatile uint32_t s_blocks      = 0;

static pdm_pcm_t s_dsp;

/* GPDMA1 CH0: SAI1_A -> pdm_ring, kruhovy linked-list (1 uzel), preruseni
   v polovine a na konci bufferu. Port MIC_DMA_Init z Mik_stm. */
void mic_dma_init(void)
{
  DMA_NodeConfTypeDef node = {0};

  __HAL_RCC_GPDMA1_CLK_ENABLE();

  node.NodeType                        = DMA_GPDMA_LINEAR_NODE;
  node.Init.Request                    = GPDMA1_REQUEST_SAI1_A;
  node.Init.BlkHWRequest               = DMA_BREQ_SINGLE_BURST;
  node.Init.Direction                  = DMA_PERIPH_TO_MEMORY;
  node.Init.SrcInc                     = DMA_SINC_FIXED;
  node.Init.DestInc                    = DMA_DINC_INCREMENTED;
  node.Init.SrcDataWidth               = DMA_SRC_DATAWIDTH_HALFWORD;
  node.Init.DestDataWidth              = DMA_DEST_DATAWIDTH_HALFWORD;
  node.Init.SrcBurstLength             = 1;
  node.Init.DestBurstLength            = 1;
  node.Init.Priority                   = DMA_HIGH_PRIORITY;
  node.Init.TransferAllocatedPort      = DMA_SRC_ALLOCATED_PORT0 | DMA_DEST_ALLOCATED_PORT0;
  node.Init.TransferEventMode          = DMA_TCEM_BLOCK_TRANSFER;
  node.Init.Mode                       = DMA_NORMAL;
  node.DataHandlingConfig.DataExchange = DMA_EXCHANGE_NONE;
  node.DataHandlingConfig.DataAlignment = DMA_DATA_RIGHTALIGN_ZEROPADDED;
  node.SrcAddress                      = (uint32_t)&SAI1_Block_A->DR;
  node.DstAddress                      = (uint32_t)pdm_ring;
  node.DataSize                        = sizeof(pdm_ring);
  if (HAL_DMAEx_List_BuildNode(&node, &pdm_dma_node) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMAEx_List_InsertNode_Tail(&pdm_dma_queue, &pdm_dma_node) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMAEx_List_SetCircularMode(&pdm_dma_queue) != HAL_OK)
  {
    Error_Handler();
  }

  hdma_sai_rx.Instance                         = GPDMA1_Channel0;
  hdma_sai_rx.InitLinkedList.Priority          = DMA_HIGH_PRIORITY;
  hdma_sai_rx.InitLinkedList.LinkStepMode      = DMA_LSM_FULL_EXECUTION;
  hdma_sai_rx.InitLinkedList.LinkAllocatedPort = DMA_LINK_ALLOCATED_PORT0;
  hdma_sai_rx.InitLinkedList.TransferEventMode = DMA_TCEM_BLOCK_TRANSFER;
  hdma_sai_rx.InitLinkedList.LinkedListMode    = DMA_LINKEDLIST_CIRCULAR;
  if (HAL_DMAEx_List_Init(&hdma_sai_rx) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_DMAEx_List_LinkQ(&hdma_sai_rx, &pdm_dma_queue) != HAL_OK)
  {
    Error_Handler();
  }
  __HAL_LINKDMA(&hsai_BlockA1, hdmarx, hdma_sai_rx);

  HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
}

int mic_start(void)
{
  pdm_pcm_init(&s_dsp, PDM_SLOT_MASK_A);
  s_half0_ready = 0;
  s_half1_ready = 0;
  s_overrun     = 0;
  s_blocks      = 0;
  s_running     = 1;

  if (HAL_SAI_Receive_DMA(&hsai_BlockA1, (uint8_t *)pdm_ring,
                          PDM_RING_HALFWORDS) != HAL_OK)
  {
    s_running = 0;
    return HAL_ERROR;
  }
  return HAL_OK;
}

void mic_stop(void)
{
  s_running = 0;
  HAL_SAI_DMAStop(&hsai_BlockA1);
}

bool mic_poll(int16_t *pcm, size_t *nsamp)
{
  const uint16_t *src;

  if (s_half0_ready)
  {
    src = &pdm_ring[0];
    s_half0_ready = 0;
  }
  else if (s_half1_ready)
  {
    src = &pdm_ring[PDM_RING_HALFWORDS / 2u];
    s_half1_ready = 0;
  }
  else
  {
    return false;
  }

  pdm_pcm_process_half(&s_dsp, src, pcm);
  if (nsamp)
  {
    *nsamp = PCM_SAMPLES_PER_HALF;
  }
  s_blocks++;
  return true;
}

bool mic_overrun(void)
{
  return s_overrun != 0;
}

uint32_t mic_blocks_processed(void)
{
  return s_blocks;
}

/* --- HAL callbacky: prvni/druha polovina kruhoveho bufferu hotova ---------- */
void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
  (void)hsai;
  if (s_running)
  {
    if (s_half0_ready)
    {
      s_overrun = 1; /* main nestihl zpracovat predchozi prvni polovinu */
    }
    s_half0_ready = 1;
  }
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
  (void)hsai;
  if (s_running)
  {
    if (s_half1_ready)
    {
      s_overrun = 1;
    }
    s_half1_ready = 1;
  }
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai)
{
  (void)hsai;
  s_overrun = 1; /* obvykle SAI overrun */
}

/* GPDMA1 Channel0 preruseni -> HAL DMA obsluha (modul si handler vlastni, aby
   byl soberestacny; CubeMX nesmi generovat vlastni handler pro tento kanal). */
void GPDMA1_Channel0_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_sai_rx);
}
