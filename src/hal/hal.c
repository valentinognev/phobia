#include <stddef.h>

#include "hal.h"
#include "libc.h"

#include "cmsis/stm32xx.h"

#define HAL_FLAG_SIGNATURE	0x2A7CEA64U
#define HAL_TEXT_INC(np)	(((np) < sizeof(log.text) - 1U) ? (np) + 1 : 0)

uint32_t			clock_cpu_hz;

HAL_t				hal;
LOG_t				log		LD_NOINIT;

typedef struct {

	uint32_t	bootload_flag;
	uint32_t	crystal_disabled;
}
priv_HAL_t;

static volatile priv_HAL_t	noinit_HAL	LD_NOINIT;

void irq_NMI()
{
	log_TRACE("IRQ NMI" EOL);

	if (RCC->CIR & RCC_CIR_CSSF) {

		noinit_HAL.crystal_disabled = HAL_FLAG_SIGNATURE;

		RCC->CIR |= RCC_CIR_CSSC;

		log_TRACE("HSE clock fault" EOL);
	}

	hal_system_reset();
}

void irq_HardFault()
{
	log_TRACE("IRQ HardFault" EOL);

	hal_system_reset();
}

void irq_MemoryFault()
{
	log_TRACE("IRQ MemoryFault" EOL);

	hal_system_reset();
}

void irq_BusFault()
{
	log_TRACE("IRQ BusFault" EOL);

	hal_system_reset();
}

void irq_UsageFault()
{
	log_TRACE("IRQ UsageFault" EOL);

	hal_system_reset();
}

void irq_Default()
{
	log_TRACE("IRQ Default" EOL);

	hal_system_reset();
}

static void
core_startup()
{
	uint32_t	CLOCK, PLLQ, PLLP, PLLN, PLLM;

	/* Vector table offset.
	 * */
	SCB->VTOR = (uint32_t) &ld_begin_text;

	/* Configure priority grouping.
	 * */
	NVIC_SetPriorityGrouping(0U);

	/* Enable HSI.
	 * */
	RCC->CR |= RCC_CR_HSION;

	/* Reset RCC.
	 * */
	RCC->CR &= ~(RCC_CR_PLLI2SON | RCC_CR_PLLON | RCC_CR_CSSON
			| RCC_CR_HSEBYP | RCC_CR_HSEON);

	RCC->PLLCFGR = 0;
	RCC->CFGR = 0;
	RCC->CIR = 0;

#ifdef STM32F7
	RCC->DCKCFGR1 = 0;
	RCC->DCKCFGR2 = 0;
#endif /* STM32F7 */

	if (noinit_HAL.crystal_disabled != HAL_FLAG_SIGNATURE) {

		int		N = 0;

		/* Enable HSE.
		 * */
		RCC->CR |= RCC_CR_HSEON;

		/* Wait till HSE is ready.
		 * */
		do {
			if ((RCC->CR & RCC_CR_HSERDY) == RCC_CR_HSERDY)
				break;

			__NOP();
			__NOP();

			if (N > 70000U) {

				log_TRACE("HSE not ready" EOL);

				noinit_HAL.crystal_disabled = HAL_FLAG_SIGNATURE;

#ifdef STM32F7
				/* D-Cache Clean and Invalidate.
				 * */
				SCB->DCCIMVAC = (uint32_t) &noinit_HAL.crystal_disabled;

				__DSB();
				__ISB();

#endif /* STM32F7 */
				break;
			}

			N++;
		}
		while (1);
	}

	/* Enable power interface clock.
	 * */
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;

	/* Regulator voltage scale 1 mode.
	 * */
#if defined(STM32F4)
	PWR->CR |= PWR_CR_VOS;
#elif defined(STM32F7)
	PWR->CR1 |= PWR_CR1_VOS_1 | PWR_CR1_VOS_0;
#endif /* STM32Fx */

	/* Set AHB/APB1/APB2 prescalers.
	 * */
	RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;

	if (noinit_HAL.crystal_disabled != HAL_FLAG_SIGNATURE) {

		CLOCK = HW_CLOCK_CRYSTAL_HZ;

		/* Clock from HSE.
		 * */
		RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSE;

		/* Enable CSS.
		 * */
		RCC->CR |= RCC_CR_CSSON;
	}
	else {
		CLOCK = 16000000U;

		/* Clock from HSI.
		 * */
		RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSI;
	}

#if defined(STM32F4)
	clock_cpu_hz = 168000000U;
#elif defined(STM32F7)
	clock_cpu_hz = 216000000U;
#endif /* STM32Fx */

	PLLP = 2;

	PLLM = (CLOCK + 1999999U) / 2000000U;
	CLOCK /= PLLM;

	PLLN = (PLLP * clock_cpu_hz) / CLOCK;
	CLOCK *= PLLN;

	PLLQ = (CLOCK + 47999999U) / 48000000U;

	RCC->PLLCFGR |= (PLLQ << RCC_PLLCFGR_PLLQ_Pos)
		| ((PLLP / 2U - 1U) << RCC_PLLCFGR_PLLP_Pos)
		| (PLLN << RCC_PLLCFGR_PLLN_Pos)
		| (PLLM << RCC_PLLCFGR_PLLM_Pos);

	/* Get actual clock frequency.
	 * */
	clock_cpu_hz = CLOCK / PLLP;

	/* Enable PLL.
	 * */
	RCC->CR |= RCC_CR_PLLON;

	/* Wait till the main PLL is ready.
	 * */
	while ((RCC->CR & RCC_CR_PLLRDY) != RCC_CR_PLLRDY) {

		__NOP();
	}

	/* Configure Flash.
	 * */
#if defined(STM32F4)
	FLASH->ACR = FLASH_ACR_DCEN | FLASH_ACR_ICEN
		| FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_5WS;
#elif defined(STM32F7)
	FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_LATENCY_5WS;
#endif /* STM32Fx */

	/* Select PLL.
	 * */
	RCC->CFGR |= RCC_CFGR_SW_PLL;

	/* Wait till PLL is used.
	 * */
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {

		__NOP();
	}

	/* Enable caching on Cortex-M7.
	 * */
#ifdef STM32F7
	SCB_EnableICache();
	SCB_EnableDCache();
#endif /* STM32F7 */
}

static void
periph_startup()
{
	/* Enable LSI.
	 * */
	RCC->CSR |= RCC_CSR_LSION;

	/* Enable GPIO clock.
	 * */
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;

	/* Enable DMA clock.
	 * */
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

	/* Check for reset reason.
	 * */
#if defined(STM32F4)
	if (RCC->CSR & RCC_CSR_WDGRSTF) {
#elif defined(STM32F7)
	if (RCC->CSR & RCC_CSR_IWDGRSTF) {
#endif /* STM32Fx */

		log_TRACE("RESET on WD" EOL);
	}

	RCC->CSR |= RCC_CSR_RMVF;
}

static void
flash_verify()
{
	uint32_t	flash_sizeof, *flash_crc32, crc32;

	flash_sizeof = fw.ld_end - fw.ld_begin;
	flash_crc32 = (uint32_t *) fw.ld_end;

	if (*flash_crc32 == 0xFFFFFFFFU) {

		crc32 = crc32b((const void *) fw.ld_begin, flash_sizeof);

		/* Update flash CRC32.
		 * */
		FLASH_prog(flash_crc32, crc32);
	}

	if (crc32b((const void *) fw.ld_begin, flash_sizeof) != *flash_crc32) {

		log_TRACE("FLASH CRC32 invalid" EOL);
	}
}

void hal_bootload()
{
	const uint32_t		*sysmem;

	if (noinit_HAL.bootload_flag == HAL_FLAG_SIGNATURE) {

		noinit_HAL.bootload_flag = 0U;

#if defined(STM32F4)
		sysmem = (const uint32_t *) 0x1FFF0000U;
#elif defined(STM32F7)
		sysmem = (const uint32_t *) 0x1FF00000U;
#endif /* STM32Fx */

		/* Load MSP.
		 * */
		__set_MSP(sysmem[0]);

		/* Jump to the bootloader.
		 * */
		((void (*) (void)) (sysmem[1])) ();
	}

	/* Enable FPU early.
	 * */
	SCB->CPACR |= (0xFU << 20);

	__DSB();
	__ISB();
}

void hal_startup()
{
	core_startup();
	periph_startup();
	flash_verify();
}

int hal_lock_irq()
{
	int		irq;

	irq = __get_BASEPRI();
	__set_BASEPRI(1 << (8 - __NVIC_PRIO_BITS));

	__DSB();
	__ISB();

	return irq;
}

void hal_unlock_irq(int irq)
{
	__set_BASEPRI(irq);
}

void hal_system_reset()
{
#ifdef STM32F7
	SCB_CleanDCache();
#endif /* STM32F7 */

	NVIC_SystemReset();
}

void hal_bootload_jump()
{
	noinit_HAL.bootload_flag = HAL_FLAG_SIGNATURE;

	hal_system_reset();
}

void hal_cpu_sleep()
{
	__DSB();
	__WFI();
}

void hal_memory_fence()
{
	__DMB();
}

int log_status()
{
	return (	log.boot_FLAG == HAL_FLAG_SIGNATURE
			&& log.text_wp != log.text_rp) ? HAL_FAULT : HAL_OK;
}

void log_bootup()
{
	if (log.boot_FLAG != HAL_FLAG_SIGNATURE) {

		log.boot_FLAG = HAL_FLAG_SIGNATURE;
		log.boot_COUNT = 0U;

		log.text_wp = 0;
		log.text_rp = 0;
	}
	else {
		log.boot_COUNT += 1U;
	}
}

void log_putc(int c)
{
	if (unlikely(log.boot_FLAG != HAL_FLAG_SIGNATURE)) {

		log.boot_FLAG = HAL_FLAG_SIGNATURE;

		log.text_wp = 0;
		log.text_rp = 0;
	}

	log.text[log.text_wp] = (char) c;

	log.text_wp = HAL_TEXT_INC(log.text_wp);
	log.text_rp = (log.text_rp == log.text_wp)
		? HAL_TEXT_INC(log.text_rp) : log.text_rp;
}

void log_flush()
{
	int		rp, wp;

	if (log.boot_FLAG == HAL_FLAG_SIGNATURE) {

		rp = log.text_rp;
		wp = log.text_wp;

		while (rp != wp) {

			putc(log.text[rp]);

			rp = HAL_TEXT_INC(rp);
		}

		puts(EOL);
	}
}

void log_clean()
{
	if (unlikely(log.boot_FLAG != HAL_FLAG_SIGNATURE)) {

		log.boot_FLAG = HAL_FLAG_SIGNATURE;
	}

	log.text_wp = 0;
	log.text_rp = 0;
}

void DBGMCU_mode_stop()
{
	DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;
	DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM1_STOP;
}

