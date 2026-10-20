#define I2C_CR2_ITEVTEN I2C_CR2_ITEVTEN_Msk                /*!< Event Interrupt Enable */
#define I2C_CR2_ITBUFEN I2C_CR2_ITBUFEN_Msk                /*!< Buffer Interrupt Enable */
#define I2C_CR2_ITERREN I2C_CR2_ITERREN_Msk                /*!< Error Interrupt Enable */
#define I2C_SR1_BERR I2C_SR1_BERR_Msk                   /*!< Bus Error */
#define I2C_SR1_ARLO I2C_SR1_ARLO_Msk                   /*!< Arbitration Lost (master mode) */
#define I2C_SR1_AF I2C_SR1_AF_Msk                     /*!< Acknowledge Failure */
#define I2C_SR1_OVR I2C_SR1_OVR_Msk                    /*!< Overrun/Underrun */
#define RCC_APB1ENR_I2C1EN RCC_APB1ENR_I2C1EN_Msk            /*!< I2C 1 clock enable */
#define RCC_APB1RSTR_I2C1RST RCC_APB1RSTR_I2C1RST_Msk          /*!< I2C 1 reset */
#define RCC_APB2ENR_AFIOEN RCC_APB2ENR_AFIOEN_Msk            /*!< Alternate Function I/O clock enable */
#define RCC_APB2ENR_IOPBEN RCC_APB2ENR_IOPBEN_Msk            /*!< I/O port B clock enable */
#define GPIO_BSRR_BS6 GPIO_BSRR_BS6_Msk                 /*!< Port x Set bit 6 */
#define GPIO_BSRR_BS7 GPIO_BSRR_BS7_Msk                 /*!< Port x Set bit 7 */
#define GPIO_CRL_MODE6 GPIO_CRL_MODE6_Msk                /*!< MODE6[1:0] bits (Port x mode bits, pin 6) */
#define GPIO_CRL_CNF6 GPIO_CRL_CNF6_Msk                 /*!< CNF6[1:0] bits (Port x configuration bits, pin 6) */
#define GPIO_CRL_MODE7 GPIO_CRL_MODE7_Msk                /*!< MODE7[1:0] bits (Port x mode bits, pin 7) */
#define GPIO_CRL_CNF7 GPIO_CRL_CNF7_Msk                 /*!< CNF7[1:0] bits (Port x configuration bits, pin 7) */
#define GPIO_CRL_MODE6_1 (0x2UL << GPIO_CRL_MODE6_Pos)      /*!< 0x02000000 */
#define GPIO_CRL_MODE7_1 (0x2UL << GPIO_CRL_MODE7_Pos)      /*!< 0x20000000 */
#define RCC_CFGR_PPRE1 RCC_CFGR_PPRE1_Msk                /*!< PRE1[2:0] bits (APB1 prescaler) */
#define RCC_CFGR_PPRE1_DIV2 0x00000400U                       /*!< HCLK divided by 2 */
#define AFIO_MAPR_I2C1_REMAP AFIO_MAPR_I2C1_REMAP_Msk          /*!< I2C1 remapping */
#define I2C_CR1_PE I2C_CR1_PE_Msk                     /*!< Peripheral Enable */
#define GPIO_IDR_IDR6 GPIO_IDR_IDR6_Msk                 /*!< Port input data, bit 6 */
#define GPIO_IDR_IDR7 GPIO_IDR_IDR7_Msk                 /*!< Port input data, bit 7 */
#define I2C_CR1_START I2C_CR1_START_Msk                  /*!< Start Generation */
#define I2C_CR1_STOP I2C_CR1_STOP_Msk                   /*!< Stop Generation */
#define I2C_SR2_BUSY I2C_SR2_BUSY_Msk                   /*!< Bus Busy */
#define GPIO_CRL_CNF6_0 (0x1UL << GPIO_CRL_CNF6_Pos)       /*!< 0x04000000 */
#define GPIO_CRL_CNF7_0 (0x1UL << GPIO_CRL_CNF7_Pos)       /*!< 0x40000000 */
#define GPIO_BRR_BR6 GPIO_BRR_BR6_Msk                  /*!< Port x Reset bit 6 */
#define GPIO_BRR_BR7 GPIO_BRR_BR7_Msk                  /*!< Port x Reset bit 7 */
#define I2C_SR2_MSL I2C_SR2_MSL_Msk                    /*!< Master/Slave */
#define I2C_SR1_SB I2C_SR1_SB_Msk                     /*!< Start Bit (Master mode) */
#define I2C_SR1_ADDR I2C_SR1_ADDR_Msk                   /*!< Address sent (master mode)/matched (slave mode) */
#define I2C_SR1_BTF I2C_SR1_BTF_Msk                    /*!< Byte Transfer Finished */
#define I2C_SR1_TXE I2C_SR1_TXE_Msk                    /*!< Data Register Empty (transmitters) */
#define I2C_CR1_POS I2C_CR1_POS_Msk                    /*!< Acknowledge/PEC Position (for data reception) */
#define I2C_CR1_ACK I2C_CR1_ACK_Msk                    /*!< Acknowledge Enable */
#define I2C_SR1_RXNE I2C_SR1_RXNE_Msk                   /*!< Data Register not Empty (receivers) */
#define I2C_CR2_ITEVTEN_Msk (0x1UL << I2C_CR2_ITEVTEN_Pos)      /*!< 0x00000200 */
#define I2C_CR2_ITBUFEN_Msk (0x1UL << I2C_CR2_ITBUFEN_Pos)      /*!< 0x00000400 */
#define I2C_CR2_ITERREN_Msk (0x1UL << I2C_CR2_ITERREN_Pos)      /*!< 0x00000100 */
#define I2C_SR1_BERR_Msk (0x1UL << I2C_SR1_BERR_Pos)         /*!< 0x00000100 */
#define I2C_SR1_ARLO_Msk (0x1UL << I2C_SR1_ARLO_Pos)         /*!< 0x00000200 */
#define I2C_SR1_AF_Msk (0x1UL << I2C_SR1_AF_Pos)           /*!< 0x00000400 */
#define I2C_SR1_OVR_Msk (0x1UL << I2C_SR1_OVR_Pos)          /*!< 0x00000800 */
#define RCC_APB1ENR_I2C1EN_Msk (0x1UL << RCC_APB1ENR_I2C1EN_Pos)  /*!< 0x00200000 */
#define RCC_APB1RSTR_I2C1RST_Msk (0x1UL << RCC_APB1RSTR_I2C1RST_Pos) /*!< 0x00200000 */
#define RCC_APB2ENR_AFIOEN_Msk (0x1UL << RCC_APB2ENR_AFIOEN_Pos)  /*!< 0x00000001 */
#define RCC_APB2ENR_IOPBEN_Msk (0x1UL << RCC_APB2ENR_IOPBEN_Pos)  /*!< 0x00000008 */
#define GPIO_BSRR_BS6_Msk (0x1UL << GPIO_BSRR_BS6_Pos)       /*!< 0x00000040 */
#define GPIO_BSRR_BS7_Msk (0x1UL << GPIO_BSRR_BS7_Pos)       /*!< 0x00000080 */
#define GPIO_CRL_MODE6_Msk (0x3UL << GPIO_CRL_MODE6_Pos)      /*!< 0x03000000 */
#define GPIO_CRL_CNF6_Msk (0x3UL << GPIO_CRL_CNF6_Pos)       /*!< 0x0C000000 */
#define GPIO_CRL_MODE7_Msk (0x3UL << GPIO_CRL_MODE7_Pos)      /*!< 0x30000000 */
#define GPIO_CRL_CNF7_Msk (0x3UL << GPIO_CRL_CNF7_Pos)       /*!< 0xC0000000 */
#define GPIO_CRL_MODE6_Pos (24U)
#define GPIO_CRL_MODE7_Pos (28U)
#define RCC_CFGR_PPRE1_Msk (0x7UL << RCC_CFGR_PPRE1_Pos)      /*!< 0x00000700 */
#define AFIO_MAPR_I2C1_REMAP_Msk (0x1UL << AFIO_MAPR_I2C1_REMAP_Pos) /*!< 0x00000002 */
#define I2C1 ((I2C_TypeDef *)I2C1_BASE)
#define I2C_CR1_PE_Msk (0x1UL << I2C_CR1_PE_Pos)           /*!< 0x00000001 */
#define GPIO_IDR_IDR6_Msk (0x1UL << GPIO_IDR_IDR6_Pos)       /*!< 0x00000040 */
#define GPIO_IDR_IDR7_Msk (0x1UL << GPIO_IDR_IDR7_Pos)       /*!< 0x00000080 */
#define I2C_CR1_START_Msk (0x1UL << I2C_CR1_START_Pos)        /*!< 0x00000100 */
#define I2C_CR1_STOP_Msk (0x1UL << I2C_CR1_STOP_Pos)         /*!< 0x00000200 */
#define I2C_SR2_BUSY_Msk (0x1UL << I2C_SR2_BUSY_Pos)         /*!< 0x00000002 */
#define GPIO_CRL_CNF6_Pos (26U)
#define GPIO_CRL_CNF7_Pos (30U)
#define GPIO_BRR_BR6_Msk (0x1UL << GPIO_BRR_BR6_Pos)        /*!< 0x00000040 */
#define GPIO_BRR_BR7_Msk (0x1UL << GPIO_BRR_BR7_Pos)        /*!< 0x00000080 */
#define I2C_SR2_MSL_Msk (0x1UL << I2C_SR2_MSL_Pos)          /*!< 0x00000001 */
#define I2C_SR1_SB_Msk (0x1UL << I2C_SR1_SB_Pos)           /*!< 0x00000001 */
#define I2C_SR1_ADDR_Msk (0x1UL << I2C_SR1_ADDR_Pos)         /*!< 0x00000002 */
#define I2C_SR1_BTF_Msk (0x1UL << I2C_SR1_BTF_Pos)          /*!< 0x00000004 */
#define I2C_SR1_TXE_Msk (0x1UL << I2C_SR1_TXE_Pos)          /*!< 0x00000080 */
#define I2C_CR1_POS_Msk (0x1UL << I2C_CR1_POS_Pos)          /*!< 0x00000800 */
#define I2C_CR1_ACK_Msk (0x1UL << I2C_CR1_ACK_Pos)          /*!< 0x00000400 */
#define I2C_SR1_RXNE_Msk (0x1UL << I2C_SR1_RXNE_Pos)         /*!< 0x00000040 */
#define I2C_CR2_ITEVTEN_Pos (9U)
#define I2C_CR2_ITBUFEN_Pos (10U)
#define I2C_CR2_ITERREN_Pos (8U)
#define I2C_SR1_BERR_Pos (8U)
#define I2C_SR1_ARLO_Pos (9U)
#define I2C_SR1_AF_Pos (10U)
#define I2C_SR1_OVR_Pos (11U)
#define RCC_APB1ENR_I2C1EN_Pos (21U)
#define RCC_APB1RSTR_I2C1RST_Pos (21U)
#define RCC_APB2ENR_AFIOEN_Pos (0U)
#define RCC_APB2ENR_IOPBEN_Pos (3U)
#define GPIO_BSRR_BS6_Pos (6U)
#define GPIO_BSRR_BS7_Pos (7U)
#define RCC_CFGR_PPRE1_Pos (8U)
#define AFIO_MAPR_I2C1_REMAP_Pos (1U)
#define I2C1_BASE (APB1PERIPH_BASE + 0x00005400UL)
#define I2C_CR1_PE_Pos (0U)
#define GPIO_IDR_IDR6_Pos (6U)
#define GPIO_IDR_IDR7_Pos (7U)
#define I2C_CR1_START_Pos (8U)
#define I2C_CR1_STOP_Pos (9U)
#define I2C_SR2_BUSY_Pos (1U)
#define GPIO_BRR_BR6_Pos (6U)
#define GPIO_BRR_BR7_Pos (7U)
#define I2C_SR2_MSL_Pos (0U)
#define I2C_SR1_SB_Pos (0U)
#define I2C_SR1_ADDR_Pos (1U)
#define I2C_SR1_BTF_Pos (2U)
#define I2C_SR1_TXE_Pos (7U)
#define I2C_CR1_POS_Pos (11U)
#define I2C_CR1_ACK_Pos (10U)
#define I2C_SR1_RXNE_Pos (6U)
#define APB1PERIPH_BASE PERIPH_BASE
#define PERIPH_BASE 0x40000000UL /*!< Peripheral base address in the alias region */
