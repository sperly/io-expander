#include "unit.h"

int unit_failures, unit_checks;

void test_crc_modbus(void);
void test_config(void);
void test_io_model(void);
void test_app(void);

int main(void)
{
    RUN(test_crc_modbus);
    RUN(test_config);
    RUN(test_io_model);
    RUN(test_app);
    printf("%d checks, %d failures\n", unit_checks, unit_failures);
    return unit_failures ? 1 : 0;
}
