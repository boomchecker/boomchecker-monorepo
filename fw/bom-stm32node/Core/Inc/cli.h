/**
 ******************************************************************************
 * @file    cli.h
 * @brief   Interaktivni konzole nad embedded-cli (funbiscuit).
 *
 * Transportne nezavisle: vystup jde pres callback cli_tx_fn, vstup se krmi
 * pres cli_feed(). USB CDC glue (CDC_Transmit_FS / CDC_Receive_FS) zije mimo
 * tento modul, takze cli.c nema zavislost na USB stacku.
 ******************************************************************************
 */
#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Transportni odeslani: posle `len` bajtu.
 * @return 0 pri uspechu; nenulove = zaneprazdneno (cli_process zkusi tyz blok
 *         znovu v dalsi iteraci). Pro USB CDC obaluje CDC_Transmit_FS.
 */
typedef int (*cli_tx_fn)(const uint8_t *buf, uint16_t len);

/**
 * @brief Vytvori CLI (staticky buffer), zaregistruje prikazy, vypise pozvanku.
 * @param tx transportni odesilaci callback (nesmi byt NULL pro vystup)
 */
void cli_init(cli_tx_fn tx);

/**
 * @brief Zpracuje prikazy a vyprazdni TX buffer. Volat v hlavni smycce.
 */
void cli_process(void);

/**
 * @brief Preda prijate bajty do CLI. Volat z RX (napr. CDC_Receive_FS).
 */
void cli_feed(const uint8_t *buf, uint32_t len);

#endif /* CLI_H */
