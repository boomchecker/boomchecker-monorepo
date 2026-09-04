/**
 ******************************************************************************
 * @file    gps.c
 * @brief   Teseo-LIV3R NMEA passthrough (see gps.h).
 *
 * RX path: UART4 RXNE interrupt -> byte ring -> line assembly in gps_run(),
 * which runs synchronously inside the CLI binding like detector_run(). The
 * handler lives here (startup vectors are weak), NVIC is enabled from user
 * code so the CubeMX files stay untouched - same pattern as mic.c/GPDMA.
 ******************************************************************************
 */
#include "gps.h"
#include "usart.h"   /* huart4 */
#include "usb_cli.h" /* usb_cli_pump / connected / write_blocking */
#include "main.h"    /* HAL_GetTick */

#include <stdio.h>
#include <string.h>

/* At 9600 Bd the module produces ~960 B/s; one console write of an 80-byte
   line returns in well under a millisecond, so 1 KB of slack is plenty even
   for a 115200 Bd scan. */
#define GPS_RING_LEN 1024u
#define GPS_LINE_MAX 128u

static volatile uint8_t  s_ring[GPS_RING_LEN];
static volatile uint16_t s_head;    /* written by the IRQ handler          */
static volatile uint16_t s_tail;    /* consumed by gps_run                 */
static volatile uint32_t s_overrun; /* ring full: incoming byte dropped    */
/* Per-flag UART error counters - separated to tell marginal signal levels
   (NE) apart from baud mismatch (FE) and IRQ starvation (ORE). */
static volatile uint32_t s_err_ne, s_err_fe, s_err_ore, s_err_pe;

static uint32_t s_cur_baud; /* 0 = UART still at the CubeMX boot default */

void UART4_IRQHandler(void)
{
  uint32_t isr = UART4->ISR;

  if (isr & (USART_ISR_ORE | USART_ISR_FE | USART_ISR_NE | USART_ISR_PE))
  {
    UART4->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF
               | USART_ICR_PECF;
    if (isr & USART_ISR_NE)  { s_err_ne++;  }
    if (isr & USART_ISR_FE)  { s_err_fe++;  }
    if (isr & USART_ISR_ORE) { s_err_ore++; }
    if (isr & USART_ISR_PE)  { s_err_pe++;  }
  }
  if (isr & USART_ISR_RXNE_RXFNE)
  {
    uint8_t  b    = (uint8_t)UART4->RDR; /* clears RXNE */
    uint16_t next = (uint16_t)((s_head + 1u) % GPS_RING_LEN);
    if (next != s_tail)
    {
      s_ring[s_head] = b;
      s_head         = next;
    }
    else
    {
      s_overrun++;
    }
  }
}

/* Bring UART4 to `baud` and make sure interrupt reception is running.
   Reception stays enabled after every command (the ring simply drops bytes
   once full) so a reply the module sends between `gpstx` and the next `gps`
   is not lost. The ring is cleared only when the baud changes (a re-init
   garbles any partial byte anyway); the error counters reset per call. */
static int gps_uart_start(uint32_t baud)
{
  HAL_NVIC_DisableIRQ(UART4_IRQn);

  if (baud != s_cur_baud)
  {
    huart4.Init.BaudRate = baud;
    if (HAL_UART_Init(&huart4) != HAL_OK)
    {
      return -1;
    }
    s_cur_baud = baud;
    s_head     = 0u;
    s_tail     = 0u;
  }

  s_overrun = 0u;
  s_err_ne  = 0u;
  s_err_fe  = 0u;
  s_err_ore = 0u;
  s_err_pe  = 0u;

  __HAL_UART_CLEAR_FLAG(&huart4, UART_CLEAR_OREF | UART_CLEAR_FEF
                                | UART_CLEAR_NEF | UART_CLEAR_PEF);
  SET_BIT(UART4->CR1, USART_CR1_RXNEIE_RXFNEIE);
  HAL_NVIC_SetPriority(UART4_IRQn, 7, 0); /* below USB (0) and mic DMA (5) */
  HAL_NVIC_EnableIRQ(UART4_IRQn);
  return 0;
}

static void gps_print(const char *line)
{
  (void)usb_cli_write_blocking((const uint8_t *)line, (uint32_t)strlen(line));
}

void gps_run(uint32_t seconds, uint32_t baud)
{
  char     line[GPS_LINE_MAX + 2u]; /* room for '\n' + NUL */
  uint32_t line_len = 0u;
  uint32_t lines = 0u, bytes = 0u;
  uint8_t  err = 0u;

  if (!usb_cli_connected() || seconds == 0u)
  {
    return;
  }
  if (seconds > GPS_MAX_SECONDS)
  {
    seconds = GPS_MAX_SECONDS;
  }

  /* NOTE: no usb_cli_flush_tx() here - flushing the console ring from inside
     a CLI binding wedges the CDC write state machine (see detector.c). */

  if (gps_uart_start(baud) != 0)
  {
    gps_print("GPSERR uart init failed\n");
    gps_print("GPSEND lines=0 bytes=0 ne=0 fe=0 ore=0 pe=0 overrun=0 err=1\n");
    return;
  }

  char hdr[48];
  snprintf(hdr, sizeof(hdr), "GPS baud=%lu sec=%lu\n",
           (unsigned long)baud, (unsigned long)seconds);
  gps_print(hdr);

  const uint32_t t_end = HAL_GetTick() + seconds * 1000u;

  while ((int32_t)(t_end - HAL_GetTick()) > 0)
  {
    usb_cli_pump();
    if (!usb_cli_connected())
    {
      err = 1u;
      break;
    }

    while (s_tail != s_head)
    {
      uint8_t b = s_ring[s_tail];
      s_tail    = (uint16_t)((s_tail + 1u) % GPS_RING_LEN);
      bytes++;

      if (b == (uint8_t)'\n' || line_len >= GPS_LINE_MAX)
      {
        while (line_len > 0u && (line[line_len - 1u] == '\r'
                                 || line[line_len - 1u] == '\n'))
        {
          line_len--;
        }
        if (line_len > 0u)
        {
          line[line_len]      = '\n';
          line[line_len + 1u] = '\0';
          gps_print(line);
          lines++;
        }
        line_len = 0u;
        if (b != (uint8_t)'\n')
        {
          line[line_len++] = (char)b; /* first byte of the overlong tail */
        }
      }
      else if (b != (uint8_t)'\r')
      {
        line[line_len++] = (char)b;
      }
    }
  }

  char trailer[112];
  snprintf(trailer, sizeof(trailer),
           "GPSEND lines=%lu bytes=%lu ne=%lu fe=%lu ore=%lu pe=%lu "
           "overrun=%lu err=%u\n",
           (unsigned long)lines, (unsigned long)bytes, (unsigned long)s_err_ne,
           (unsigned long)s_err_fe, (unsigned long)s_err_ore,
           (unsigned long)s_err_pe, (unsigned long)s_overrun, err);
  gps_print(trailer);
}

void gps_reset_pulse(void)
{
  HAL_GPIO_WritePin(GPS_RESET_GPIO_Port, GPS_RESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(100u);
  HAL_GPIO_WritePin(GPS_RESET_GPIO_Port, GPS_RESET_Pin, GPIO_PIN_SET);
}

int gps_send(const char *sentence, uint32_t baud)
{
  char buf[GPS_LINE_MAX];

  if (sentence == NULL || sentence[0] == '\0')
  {
    return -1;
  }
  if (gps_uart_start(baud) != 0) /* also arms RX so the reply is captured */
  {
    return -1;
  }

  const char *body = (sentence[0] == '$') ? sentence + 1 : sentence;
  uint8_t     csum = 0u;
  for (const char *p = body; *p != '\0'; p++)
  {
    csum ^= (uint8_t)*p;
  }
  int n = snprintf(buf, sizeof(buf), "$%s*%02X\r\n", body, csum);
  if (n <= 0 || (size_t)n >= sizeof(buf))
  {
    return -1;
  }
  if (HAL_UART_Transmit(&huart4, (const uint8_t *)buf, (uint16_t)n, 200u)
      != HAL_OK)
  {
    return -2;
  }
  return 0;
}
