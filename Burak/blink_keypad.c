#include "stm32f446xx.h"

/* Function prototypes */
static void LED_Init(void);
static void Keypad_Init(void);
static char Keypad_GetKey(void);
static void Blink_N_Times(int n);
static void delay_ms(uint32_t ms);


int main(void)
{
    LED_Init();
    Keypad_Init();

    while (1)
    {
        char key = Keypad_GetKey();

        /* Number keys 1-9 */
        if (key >= '1' && key <= '9')
        {
            Blink_N_Times(key - '0');

            /* Wait until key is released */
            while (Keypad_GetKey() != '\0')
            {
            }

            delay_ms(50);
        }

        /* 0 key */
        else if (key == '0')
        {
            Blink_N_Times(10);

            while (Keypad_GetKey() != '\0')
            {
            }

            delay_ms(50);
        }

        /* Optional A-D test patterns */
        else if (key == 'A')
        {
            Blink_N_Times(1);

            while (Keypad_GetKey() != '\0')
            {
            }

            delay_ms(50);
        }

        else if (key == 'B')
        {
            Blink_N_Times(2);

            while (Keypad_GetKey() != '\0')
            {
            }

            delay_ms(50);
        }

        else if (key == 'C')
        {
            Blink_N_Times(3);

            while (Keypad_GetKey() != '\0')
            {
            }

            delay_ms(50);
        }

        else if (key == 'D')
        {
            Blink_N_Times(4);

            while (Keypad_GetKey() != '\0')
            {
            }

            delay_ms(50);
        }
    }
}


/* -----------------------------------------------------------
 * LED INITIALIZATION
 * NUCLEO-F446RE onboard LED = PA5
 * ----------------------------------------------------------- */
static void LED_Init(void)
{
    /* Enable GPIOA clock */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    /* PA5 = General purpose output */
    GPIOA->MODER &= ~(3U << (5 * 2));
    GPIOA->MODER |=  (1U << (5 * 2));

    /* Push-pull output */
    GPIOA->OTYPER &= ~(1U << 5);

    /* Start LED OFF */
    GPIOA->BSRR = (1U << (5 + 16));
}


/* -----------------------------------------------------------
 * KEYPAD INITIALIZATION
 *
 * Rows:
 * R1 = PA10
 * R2 = PB3
 * R3 = PB5
 * R4 = PB4
 *
 * Columns:
 * C1 = PB10
 * C2 = PA8
 * C3 = PA9
 * C4 = PC7
 * ----------------------------------------------------------- */
static void Keypad_Init(void)
{
    /* Enable GPIO A, B, C clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;


    /* =======================================================
     * ROWS = OUTPUTS
     * ======================================================= */

    /* PA10 = R1 */
    GPIOA->MODER &= ~(3U << (10 * 2));
    GPIOA->MODER |=  (1U << (10 * 2));

    /* PB3 = R2 */
    GPIOB->MODER &= ~(3U << (3 * 2));
    GPIOB->MODER |=  (1U << (3 * 2));

    /* PB5 = R3 */
    GPIOB->MODER &= ~(3U << (5 * 2));
    GPIOB->MODER |=  (1U << (5 * 2));

    /* PB4 = R4 */
    GPIOB->MODER &= ~(3U << (4 * 2));
    GPIOB->MODER |=  (1U << (4 * 2));


    /* Make row outputs push-pull */
    GPIOA->OTYPER &= ~(1U << 10);

    GPIOB->OTYPER &= ~(
        (1U << 3) |
        (1U << 4) |
        (1U << 5)
    );


    /* =======================================================
     * COLUMNS = INPUTS
     * ======================================================= */

    /* PB10 = C1 */
    GPIOB->MODER &= ~(3U << (10 * 2));

    /* PA8 = C2 */
    GPIOA->MODER &= ~(3U << (8 * 2));

    /* PA9 = C3 */
    GPIOA->MODER &= ~(3U << (9 * 2));

    /* PC7 = C4 */
    GPIOC->MODER &= ~(3U << (7 * 2));


    /* =======================================================
     * COLUMN INPUTS = PULL-UP
     * ======================================================= */

    /* PB10 = pull-up */
    GPIOB->PUPDR &= ~(3U << (10 * 2));
    GPIOB->PUPDR |=  (1U << (10 * 2));

    /* PA8 = pull-up */
    GPIOA->PUPDR &= ~(3U << (8 * 2));
    GPIOA->PUPDR |=  (1U << (8 * 2));

    /* PA9 = pull-up */
    GPIOA->PUPDR &= ~(3U << (9 * 2));
    GPIOA->PUPDR |=  (1U << (9 * 2));

    /* PC7 = pull-up */
    GPIOC->PUPDR &= ~(3U << (7 * 2));
    GPIOC->PUPDR |=  (1U << (7 * 2));


    /* Set all rows HIGH initially */
    GPIOA->BSRR = (1U << 10);

    GPIOB->BSRR =
        (1U << 3) |
        (1U << 4) |
        (1U << 5);
}


/* -----------------------------------------------------------
 * KEYPAD SCANNING
 *
 * Returns:
 * '1' ... '9'
 * '0'
 * 'A' ... 'D'
 * '*'
 * '#'
 *
 * Returns '\0' if nothing is pressed
 * ----------------------------------------------------------- */
static char Keypad_GetKey(void)
{
    static const char keys[4][4] =
    {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };


    for (int row = 0; row < 4; row++)
    {
        /* -----------------------------------------------
         * Set ALL rows HIGH
         * ----------------------------------------------- */
        GPIOA->BSRR = (1U << 10);

        GPIOB->BSRR =
            (1U << 3) |
            (1U << 4) |
            (1U << 5);


        /* -----------------------------------------------
         * Pull one row LOW
         * ----------------------------------------------- */

        if (row == 0)
        {
            /* R1 = PA10 LOW */
            GPIOA->BSRR = (1U << (10 + 16));
        }

        else if (row == 1)
        {
            /* R2 = PB3 LOW */
            GPIOB->BSRR = (1U << (3 + 16));
        }

        else if (row == 2)
        {
            /* R3 = PB5 LOW */
            GPIOB->BSRR = (1U << (5 + 16));
        }

        else if (row == 3)
        {
            /* R4 = PB4 LOW */
            GPIOB->BSRR = (1U << (4 + 16));
        }


        /* Small settling delay */
        for (volatile uint32_t i = 0; i < 100; i++)
        {
        }


        /* -----------------------------------------------
         * Read columns
         *
         * Columns normally HIGH because of pull-ups.
         * Pressed key connects column to active LOW row.
         * Therefore pressed column reads LOW.
         * ----------------------------------------------- */

        /* C1 = PB10 */
        if ((GPIOB->IDR & (1U << 10)) == 0)
        {
            return keys[row][0];
        }

        /* C2 = PA8 */
        if ((GPIOA->IDR & (1U << 8)) == 0)
        {
            return keys[row][1];
        }

        /* C3 = PA9 */
        if ((GPIOA->IDR & (1U << 9)) == 0)
        {
            return keys[row][2];
        }

        /* C4 = PC7 */
        if ((GPIOC->IDR & (1U << 7)) == 0)
        {
            return keys[row][3];
        }
    }

    return '\0';
}


/* -----------------------------------------------------------
 * BLINK LED N TIMES
 * ----------------------------------------------------------- */
static void Blink_N_Times(int n)
{
    for (int i = 0; i < n; i++)
    {
        /* LED ON */
        GPIOA->BSRR = (1U << 5);

        delay_ms(10);

        /* LED OFF */
        GPIOA->BSRR = (1U << (5 + 16));

        delay_ms(10);
    }
}


/* -----------------------------------------------------------
 * SIMPLE SOFTWARE DELAY
 *
 * This is only for testing.
 * Exact timing depends on CPU clock and compiler optimization.
 * ----------------------------------------------------------- */
static void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        for (volatile uint32_t j = 0; j < 16000; j++)
        {
        }
    }
}
