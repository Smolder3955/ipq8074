/*
 * Copyright (c) 2017 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef __MAC_HWSCH_REG_BASE_ADDRESS
#define __MAC_HWSCH_REG_BASE_ADDRESS (0x3f400)
#endif
// 0x338 (HWSCH_DEBUG_SELECT)
#define HWSCH_DEBUG_SELECT_BLOCK_SELECT_LSB                                    8
#define HWSCH_DEBUG_SELECT_BLOCK_SELECT_MSB                                    15
#define HWSCH_DEBUG_SELECT_BLOCK_SELECT_MASK                                   0xff00
#define HWSCH_DEBUG_SELECT_BLOCK_SELECT_GET(x)                                 (((x) & HWSCH_DEBUG_SELECT_BLOCK_SELECT_MASK) >> HWSCH_DEBUG_SELECT_BLOCK_SELECT_LSB)
#define HWSCH_DEBUG_SELECT_BLOCK_SELECT_SET(x)                                 (((0 | (x)) << HWSCH_DEBUG_SELECT_BLOCK_SELECT_LSB) & HWSCH_DEBUG_SELECT_BLOCK_SELECT_MASK)
#define HWSCH_DEBUG_SELECT_BLOCK_SELECT_RESET                                  0x0
#define HWSCH_DEBUG_SELECT_SIGNAL_SELECT_LSB                                   0
#define HWSCH_DEBUG_SELECT_SIGNAL_SELECT_MSB                                   7
#define HWSCH_DEBUG_SELECT_SIGNAL_SELECT_MASK                                  0xff
#define HWSCH_DEBUG_SELECT_SIGNAL_SELECT_GET(x)                                (((x) & HWSCH_DEBUG_SELECT_SIGNAL_SELECT_MASK) >> HWSCH_DEBUG_SELECT_SIGNAL_SELECT_LSB)
#define HWSCH_DEBUG_SELECT_SIGNAL_SELECT_SET(x)                                (((0 | (x)) << HWSCH_DEBUG_SELECT_SIGNAL_SELECT_LSB) & HWSCH_DEBUG_SELECT_SIGNAL_SELECT_MASK)
#define HWSCH_DEBUG_SELECT_SIGNAL_SELECT_RESET                                 0x0
#define HWSCH_DEBUG_SELECT_ADDRESS                                             (0x338 + __MAC_HWSCH_REG_BASE_ADDRESS)
#define HWSCH_DEBUG_SELECT_RSTMASK                                             0xffff
#define HWSCH_DEBUG_SELECT_RESET                                               0x0

#ifndef __MAC_PDG_REG_BASE_ADDRESS
#define __MAC_PDG_REG_BASE_ADDRESS (0x30000)
#endif
// 0x24 (PDG_DEBUG_MODULE_SELECT)
#define PDG_DEBUG_MODULE_SELECT_DEBUG_MODULE_SELECT_LSB                        0
#define PDG_DEBUG_MODULE_SELECT_DEBUG_MODULE_SELECT_MSB                        31
#define PDG_DEBUG_MODULE_SELECT_DEBUG_MODULE_SELECT_MASK                       0xffffffff
#define PDG_DEBUG_MODULE_SELECT_DEBUG_MODULE_SELECT_GET(x)                     (((x) & PDG_DEBUG_MODULE_SELECT_DEBUG_MODULE_SELECT_MASK) >> PDG_DEBUG_MODULE_SELECT_DEBUG_MODULE_SELECT_LSB)
#define PDG_DEBUG_MODULE_SELECT_DEBUG_MODULE_SELECT_SET(x)                     (((0 | (x)) << PDG_DEBUG_MODULE_SELECT_DEBUG_MODULE_SELECT_LSB) & PDG_DEBUG_MODULE_SELECT_DEBUG_MODULE_SELECT_MASK)
#define PDG_DEBUG_MODULE_SELECT_DEBUG_MODULE_SELECT_RESET                      0x0
#define PDG_DEBUG_MODULE_SELECT_ADDRESS                                        (0x24 + __MAC_PDG_REG_BASE_ADDRESS)
#define PDG_DEBUG_MODULE_SELECT_RSTMASK                                        0xffffffff
#define PDG_DEBUG_MODULE_SELECT_RESET                                          0x0

#ifndef __MAC_TXDMA_REG_BASE_ADDRESS
#define __MAC_TXDMA_REG_BASE_ADDRESS (0x30400)
#endif
// 0x10 (TXDMA_TRACEBUS)
#define TXDMA_TRACEBUS_SELECT_LSB                                              0
#define TXDMA_TRACEBUS_SELECT_MSB                                              31
#define TXDMA_TRACEBUS_SELECT_MASK                                             0xffffffff
#define TXDMA_TRACEBUS_SELECT_GET(x)                                           (((x) & TXDMA_TRACEBUS_SELECT_MASK) >> TXDMA_TRACEBUS_SELECT_LSB)
#define TXDMA_TRACEBUS_SELECT_SET(x)                                           (((0 | (x)) << TXDMA_TRACEBUS_SELECT_LSB) & TXDMA_TRACEBUS_SELECT_MASK)
#define TXDMA_TRACEBUS_SELECT_RESET                                            0x0
#define TXDMA_TRACEBUS_ADDRESS                                                 (0x10 + __MAC_TXDMA_REG_BASE_ADDRESS)
#define TXDMA_TRACEBUS_RSTMASK                                                 0xffffffff
#define TXDMA_TRACEBUS_RESET                                                   0x

#ifndef __MAC_RXDMA_REG_BASE_ADDRESS
#define __MAC_RXDMA_REG_BASE_ADDRESS (0x30800)
#endif
// 0xcc (RXDMA_TRACEBUS_CTRL)
#define RXDMA_TRACEBUS_CTRL_EN_LSB                                             2
#define RXDMA_TRACEBUS_CTRL_EN_MSB                                             2
#define RXDMA_TRACEBUS_CTRL_EN_MASK                                            0x4
#define RXDMA_TRACEBUS_CTRL_EN_GET(x)                                          (((x) & RXDMA_TRACEBUS_CTRL_EN_MASK) >> RXDMA_TRACEBUS_CTRL_EN_LSB)
#define RXDMA_TRACEBUS_CTRL_EN_SET(x)                                          (((0 | (x)) << RXDMA_TRACEBUS_CTRL_EN_LSB) & RXDMA_TRACEBUS_CTRL_EN_MASK)
#define RXDMA_TRACEBUS_CTRL_EN_RESET                                           0x1
#define RXDMA_TRACEBUS_CTRL_SELECT_LSB                                         0
#define RXDMA_TRACEBUS_CTRL_SELECT_MSB                                         1
#define RXDMA_TRACEBUS_CTRL_SELECT_MASK                                        0x3
#define RXDMA_TRACEBUS_CTRL_SELECT_GET(x)                                      (((x) & RXDMA_TRACEBUS_CTRL_SELECT_MASK) >> RXDMA_TRACEBUS_CTRL_SELECT_LSB)
#define RXDMA_TRACEBUS_CTRL_SELECT_SET(x)                                      (((0 | (x)) << RXDMA_TRACEBUS_CTRL_SELECT_LSB) & RXDMA_TRACEBUS_CTRL_SELECT_MASK)
#define RXDMA_TRACEBUS_CTRL_SELECT_RESET                                       0x0
#define RXDMA_TRACEBUS_CTRL_ADDRESS                                            (0xcc + __MAC_RXDMA_REG_BASE_ADDRESS)
#define RXDMA_TRACEBUS_CTRL_RSTMASK                                            0x7
#define RXDMA_TRACEBUS_CTRL_RESET                                              0x4


#ifndef __MAC_OLE_REG_BASE_ADDRESS
#define __MAC_OLE_REG_BASE_ADDRESS (0x30c00)
#endif
// 0x12c (OLE_DEBUGBUS_CONTROL)
#define OLE_DEBUGBUS_CONTROL_TRCBUS_SELECT_LSB                                 0
#define OLE_DEBUGBUS_CONTROL_TRCBUS_SELECT_MSB                                 4
#define OLE_DEBUGBUS_CONTROL_TRCBUS_SELECT_MASK                                0x1f
#define OLE_DEBUGBUS_CONTROL_TRCBUS_SELECT_GET(x)                              (((x) & OLE_DEBUGBUS_CONTROL_TRCBUS_SELECT_MASK) >> OLE_DEBUGBUS_CONTROL_TRCBUS_SELECT_LSB)
#define OLE_DEBUGBUS_CONTROL_TRCBUS_SELECT_SET(x)                              (((0 | (x)) << OLE_DEBUGBUS_CONTROL_TRCBUS_SELECT_LSB) & OLE_DEBUGBUS_CONTROL_TRCBUS_SELECT_MASK)
#define OLE_DEBUGBUS_CONTROL_TRCBUS_SELECT_RESET                               0x0
#define OLE_DEBUGBUS_CONTROL_ADDRESS                                           (0x12c + __MAC_OLE_REG_BASE_ADDRESS)
#define OLE_DEBUGBUS_CONTROL_RSTMASK                                           0x1f
#define OLE_DEBUGBUS_CONTROL_RESET                                             0x0

#ifndef __MAC_CRYPTO_REG_BASE_ADDRESS
#define __MAC_CRYPTO_REG_BASE_ADDRESS (0x3f000)
#endif
// 0x1c (CRYPTO_TESTBUS_SEL)
#define CRYPTO_TESTBUS_SEL_WAPI_TESTSEL_LSB                                    5
#define CRYPTO_TESTBUS_SEL_WAPI_TESTSEL_MSB                                    7
#define CRYPTO_TESTBUS_SEL_WAPI_TESTSEL_MASK                                   0xe0
#define CRYPTO_TESTBUS_SEL_WAPI_TESTSEL_GET(x)                                 (((x) & CRYPTO_TESTBUS_SEL_WAPI_TESTSEL_MASK) >> CRYPTO_TESTBUS_SEL_WAPI_TESTSEL_LSB)
#define CRYPTO_TESTBUS_SEL_WAPI_TESTSEL_SET(x)                                 (((0 | (x)) << CRYPTO_TESTBUS_SEL_WAPI_TESTSEL_LSB) & CRYPTO_TESTBUS_SEL_WAPI_TESTSEL_MASK)
#define CRYPTO_TESTBUS_SEL_WAPI_TESTSEL_RESET                                  0x0
#define CRYPTO_TESTBUS_SEL_TESTBUS_SEL_LSB                                     0
#define CRYPTO_TESTBUS_SEL_TESTBUS_SEL_MSB                                     4
#define CRYPTO_TESTBUS_SEL_TESTBUS_SEL_MASK                                    0x1f
#define CRYPTO_TESTBUS_SEL_TESTBUS_SEL_GET(x)                                  (((x) & CRYPTO_TESTBUS_SEL_TESTBUS_SEL_MASK) >> CRYPTO_TESTBUS_SEL_TESTBUS_SEL_LSB)
#define CRYPTO_TESTBUS_SEL_TESTBUS_SEL_SET(x)                                  (((0 | (x)) << CRYPTO_TESTBUS_SEL_TESTBUS_SEL_LSB) & CRYPTO_TESTBUS_SEL_TESTBUS_SEL_MASK)
#define CRYPTO_TESTBUS_SEL_TESTBUS_SEL_RESET                                   0x0
#define CRYPTO_TESTBUS_SEL_ADDRESS                                             (0x1c + __MAC_CRYPTO_REG_BASE_ADDRESS)
#define CRYPTO_TESTBUS_SEL_RSTMASK                                             0xff
#define CRYPTO_TESTBUS_SEL_RESET                                               0x0

#ifndef __MAC_TXPCU_REG_BASE_ADDRESS
#define __MAC_TXPCU_REG_BASE_ADDRESS (0x36000)
#endif
// 0x80 (TXPCU_TRACEBUS_CTRL)
#define TXPCU_TRACEBUS_CTRL_SELECT_LSB                                         0
#define TXPCU_TRACEBUS_CTRL_SELECT_MSB                                         7
#define TXPCU_TRACEBUS_CTRL_SELECT_MASK                                        0xff
#define TXPCU_TRACEBUS_CTRL_SELECT_GET(x)                                      (((x) & TXPCU_TRACEBUS_CTRL_SELECT_MASK) >> TXPCU_TRACEBUS_CTRL_SELECT_LSB)
#define TXPCU_TRACEBUS_CTRL_SELECT_SET(x)                                      (((0 | (x)) << TXPCU_TRACEBUS_CTRL_SELECT_LSB) & TXPCU_TRACEBUS_CTRL_SELECT_MASK)
#define TXPCU_TRACEBUS_CTRL_SELECT_RESET                                       0x0
#define TXPCU_TRACEBUS_CTRL_ADDRESS                                            (0x80 + __MAC_TXPCU_REG_BASE_ADDRESS)
#define TXPCU_TRACEBUS_CTRL_RSTMASK                                            0xff
#define TXPCU_TRACEBUS_CTRL_RESET                                              0x0


#ifndef __MAC_RXPCU_REG_BASE_ADDRESS
#define __MAC_RXPCU_REG_BASE_ADDRESS (0x32000)
#endif
// 0x160 (RXPCU_TEST_CFG)
#define RXPCU_TEST_CFG_TESTBUS_SEL_LSB                                         8
#define RXPCU_TEST_CFG_TESTBUS_SEL_MSB                                         10
#define RXPCU_TEST_CFG_TESTBUS_SEL_MASK                                        0x700
#define RXPCU_TEST_CFG_TESTBUS_SEL_GET(x)                                      (((x) & RXPCU_TEST_CFG_TESTBUS_SEL_MASK) >> RXPCU_TEST_CFG_TESTBUS_SEL_LSB)
#define RXPCU_TEST_CFG_TESTBUS_SEL_SET(x)                                      (((0 | (x)) << RXPCU_TEST_CFG_TESTBUS_SEL_LSB) & RXPCU_TEST_CFG_TESTBUS_SEL_MASK)
#define RXPCU_TEST_CFG_TESTBUS_SEL_RESET                                       0x0
#define RXPCU_TEST_CFG_SEL_LSB                                                 0
#define RXPCU_TEST_CFG_SEL_MSB                                                 7
#define RXPCU_TEST_CFG_SEL_MASK                                                0xff
#define RXPCU_TEST_CFG_SEL_GET(x)                                              (((x) & RXPCU_TEST_CFG_SEL_MASK) >> RXPCU_TEST_CFG_SEL_LSB)
#define RXPCU_TEST_CFG_SEL_SET(x)                                              (((0 | (x)) << RXPCU_TEST_CFG_SEL_LSB) & RXPCU_TEST_CFG_SEL_MASK)
#define RXPCU_TEST_CFG_SEL_RESET                                               0x0
#define RXPCU_TEST_CFG_ADDRESS                                                 (0x160 + __MAC_RXPCU_REG_BASE_ADDRESS)
#define RXPCU_TEST_CFG_RSTMASK                                                 0x7ff
#define RXPCU_TEST_CFG_RESET                                                   0x0
