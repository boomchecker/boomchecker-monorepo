/**
 ******************************************************************************
 * @file    mic.c
 * @brief   PDM microphone acquisition (SAI1_A + GPDMA circular). From Mik_stm.
 ******************************************************************************
 */
#include "mic.h"
#include "sai.h"          /* extern SAI_HandleTypeDef hsai_BlockA1 (from CubeMX) */
#include "main.h"         /* PDM_D1/D2 pin defines (mic_diag_run) */
#include "usb_cli.h"      /* console prints for mic_diag_run */

#include <stdio.h>
#include <string.h>

/* --- Circular DMA buffer and DMA linked-list ------------------------------- */
static uint16_t pdm_ring[PDM_RING_HALFWORDS];   /* buffer filled by DMA          */

static DMA_HandleTypeDef hdma_sai_rx;           /* GPDMA1 CH0 <- SAI1_A          */
static DMA_QListTypeDef  pdm_dma_queue;
static DMA_NodeTypeDef   pdm_dma_node;

/* --- Acquisition state: ISR sets ready flags, main drains them ------------- */
static volatile uint8_t  s_running     = 0;
static volatile uint8_t  s_half0_ready = 0;     /* first ring half ready         */
static volatile uint8_t  s_half1_ready = 0;     /* second ring half ready        */
static volatile uint8_t  s_overrun     = 0;
static volatile uint32_t s_blocks      = 0;
static volatile uint32_t s_half_count  = 0;     /* DMA half-completions (ISR)     */
static int8_t            s_expect      = -1;    /* next half to deliver (-1=latch)*/

static pdm_pcm_t s_dsp;

/* GPDMA1 CH0: SAI1_A -> pdm_ring, circular linked-list (1 node), interrupts at
   the half and the end of the buffer. Port of MIC_DMA_Init from Mik_stm. */
void mic_dma_init(void)
{
  /* Both pcm_stream and detector lazy-init the DMA; building the linked list
     twice would corrupt the node queue, so make repeated calls a no-op. */
  static uint8_t s_dma_inited = 0u;
  if (s_dma_inited)
  {
    return;
  }
  s_dma_inited = 1u;

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
  s_half_count  = 0;
  s_expect      = -1;
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

  /* Deliver halves in strict capture order (0,1,0,1,...), matching the Mik_stm
     stream loop. The DSP state (CIC integrators, FIR history) is continuous, so
     a half processed out of order corrupts every following sample. Latch onto
     whichever half completes first, then alternate; if we ever lag past a wrap
     with both halves pending, half1 is the older one and must go first. */
  if (s_expect < 0)
  {
    if (s_half0_ready)
    {
      s_expect = 0;
    }
    else if (s_half1_ready)
    {
      s_expect = 1;
    }
    else
    {
      return false;
    }
  }

  if (s_expect == 0)
  {
    if (!s_half0_ready)
    {
      return false;
    }
    src = &pdm_ring[0];
    s_half0_ready = 0;
  }
  else
  {
    if (!s_half1_ready)
    {
      return false;
    }
    src = &pdm_ring[PDM_RING_HALFWORDS / 2u];
    s_half1_ready = 0;
  }

  /* Overrun blind spot: the ready flag was cleared above, so if the DMA reaches
     the boundary that overwrites *this* half while we are still processing it,
     the tear would go unreported. Snapshot the ISR half-completion counter right
     before the ~17 ms DSP; if it advanced (>=1) by the time we finish, a DMA
     boundary landed during the read and the half we delivered may be torn.
     Healthy case (17 ms DSP < 21.33 ms half period) leaves the counter put. */
  uint32_t half_at_start = s_half_count;
  pdm_pcm_process_half(&s_dsp, src, pcm);
  if (s_half_count != half_at_start)
  {
    s_overrun = 1;
  }
  if (nsamp)
  {
    *nsamp = PCM_SAMPLES_PER_HALF;
  }
  s_expect ^= 1;
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

/* --- HAL callbacks: first/second half of the circular buffer ready --------- */
void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai)
{
  (void)hsai;
  if (s_running)
  {
    s_half_count++;
    if (s_half0_ready)
    {
      s_overrun = 1; /* main did not process the previous first half in time */
    }
    s_half0_ready = 1;
  }
}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai)
{
  (void)hsai;
  if (s_running)
  {
    s_half_count++;
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
  s_overrun = 1; /* usually a SAI overrun */
}

/* GPDMA1 Channel0 interrupt -> HAL DMA handler (the module owns this handler so
   it stays self-contained; CubeMX must not generate its own for this channel). */
void GPDMA1_Channel0_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_sai_rx);
}

/* --- Wiring diagnostics (mic bring-up) -------------------------------------- */

static void diag_print(const char *line)
{
  (void)usb_cli_write_blocking((const uint8_t *)line, (uint32_t)strlen(line));
}

/* Observe the PDM data pins directly: with CK1 running, temporarily switch
   PE6 (D1) and PE4 (D2) to GPIO inputs and count level transitions - a
   transmitting PDM mic toggles its line constantly, so toggles==0 means no
   data arrives on that pin at all (the sampling loop is slower than the
   3.072 MHz bit clock, so the count is qualitative, not a bit rate). A
   static pull-up/pull-down test with the clock stopped then distinguishes a
   tri-stated/unconnected line (follows the pull) from one driven or shorted
   (ignores it). */
void mic_diag_run(void)
{
  static const uint16_t pin_mask[2] = { PDM_D1_Pin, PDM_D2_Pin };
  /* Bit positions and labels come from the CubeMX pin macros, so a pin
     reassignment in main.h cannot leave this probing (or naming) the wrong
     line. Both pins are read from one GPIOE->IDR snapshot below, hence the
     same-port check. */
  const uint32_t pin_pos[2] = { 31u - __CLZ(pin_mask[0]), 31u - __CLZ(pin_mask[1]) };
  char pin_name[2][8];
  char line[96];

  if (PDM_D1_GPIO_Port != GPIOE || PDM_D2_GPIO_Port != GPIOE)
  {
    diag_print("MICDIAG PDM data pins are not on GPIOE - probe not ported\n");
    diag_print("MICDIAGEND err=1\n");
    return;
  }
  for (uint32_t k = 0u; k < 2u; k++)
  {
    snprintf(pin_name[k], sizeof(pin_name[k]), "PE%lu/D%lu",
             (unsigned long)pin_pos[k], (unsigned long)(k + 1u));
  }

  mic_dma_init();
  if (mic_start() != 0)
  {
    diag_print("MICDIAG mic start failed\n");
    diag_print("MICDIAGEND err=1\n");
    return;
  }
  HAL_Delay(30); /* IM67D130A: startup <= 20 ms after VDD+CLOCK */

  for (uint32_t k = 0u; k < 2u; k++) /* AF -> input, AFR stays configured */
  {
    MODIFY_REG(GPIOE->MODER, 3u << (2u * pin_pos[k]), 0u);
  }

  uint32_t togg[2] = { 0u, 0u };
  uint32_t hi[2]   = { 0u, 0u };
  uint32_t prev    = GPIOE->IDR;
  const uint32_t samples = 200000u; /* ~10 ms at 250 MHz */
  for (uint32_t i = 0u; i < samples; i++)
  {
    uint32_t idr = GPIOE->IDR;
    for (uint32_t k = 0u; k < 2u; k++)
    {
      uint32_t bit = (idr >> pin_pos[k]) & 1u;
      togg[k] += bit ^ ((prev >> pin_pos[k]) & 1u);
      hi[k]   += bit;
    }
    prev = idr;
  }

  for (uint32_t k = 0u; k < 2u; k++) /* back to AF (SAI) */
  {
    MODIFY_REG(GPIOE->MODER, 3u << (2u * pin_pos[k]), 2u << (2u * pin_pos[k]));
  }
  mic_stop();

  for (uint32_t k = 0u; k < 2u; k++)
  {
    snprintf(line, sizeof(line), "MICDIAG %s clk=on toggles=%lu hi=%lu/%lu\n",
             pin_name[k], (unsigned long)togg[k], (unsigned long)hi[k],
             (unsigned long)samples);
    diag_print(line);
  }

  /* Static pull test with the PDM clock idle. */
  for (uint32_t k = 0u; k < 2u; k++)
  {
    uint32_t pos2 = 2u * pin_pos[k];
    MODIFY_REG(GPIOE->MODER, 3u << pos2, 0u);           /* input        */
    MODIFY_REG(GPIOE->PUPDR, 3u << pos2, 1u << pos2);   /* pull-up      */
    HAL_Delay(2);
    uint32_t pu = (GPIOE->IDR & pin_mask[k]) ? 1u : 0u;
    MODIFY_REG(GPIOE->PUPDR, 3u << pos2, 2u << pos2);   /* pull-down    */
    HAL_Delay(2);
    uint32_t pd = (GPIOE->IDR & pin_mask[k]) ? 1u : 0u;
    MODIFY_REG(GPIOE->PUPDR, 3u << pos2, 0u);           /* no pull      */
    MODIFY_REG(GPIOE->MODER, 3u << pos2, 2u << pos2);   /* back to AF   */
    snprintf(line, sizeof(line),
             "MICDIAG %s clk=off pu=%lu pd=%lu (%s)\n", pin_name[k],
             (unsigned long)pu, (unsigned long)pd,
             (pu == 1u && pd == 0u) ? "floating/tri-state"
             : (pu == 0u)           ? "driven/shorted LOW"
                                    : "driven HIGH");
    diag_print(line);
  }
  diag_print("MICDIAGEND err=0\n");
}
