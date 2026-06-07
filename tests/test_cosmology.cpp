#include <cstdio>
#include <cmath>
#include "cosmology.h"

static int g_fail = 0;
#define CHECK(cond) do{ if(!(cond)){ std::printf("FAIL: %s (line %d)\n", #cond, __LINE__); ++g_fail; } }while(0)
#define CHECK_NEAR(a,b,tol) do{ double _d=std::fabs((double)(a)-(double)(b)); \
    if(_d>(tol)){ std::printf("FAIL: |%s - %s| = %g > %g (line %d)\n", #a,#b,_d,(double)(tol),__LINE__); ++g_fail; } }while(0)

int main(){
    using namespace cosmo;
    // a(t0) normalized to 1, logA(t0)=0
    CHECK_NEAR(scaleFactor(T0_YEARS), 1.0, 1e-9);
    CHECK_NEAR(logScaleFactor(T0_YEARS), 0.0, 1e-9);
    // monotonic increasing
    CHECK(scaleFactor(1e9) < scaleFactor(5e9));
    CHECK(scaleFactor(5e9) < scaleFactor(T0_YEARS));
    CHECK(scaleFactor(T0_YEARS) < scaleFactor(5e10));
    // past < 1 < future
    CHECK(scaleFactor(1e9) < 1.0);
    CHECK(scaleFactor(5e10) > 1.0);
    // extreme time stays finite in log space (no overflow/NaN)
    CHECK(std::isfinite(logScaleFactor(1e100)));
    CHECK(logScaleFactor(1e100) > 1e80);

    // redshift: z(t0)=0; past (a<1) -> z>0
    CHECK_NEAR(redshift(T0_YEARS), 0.0, 1e-9);
    CHECK(redshift(2e9) > 0.0);
    // CMB temperature: T(t0)=T_CMB0; cools toward 0 in the far future (no overflow/NaN)
    CHECK_NEAR(cmbTemperature(T0_YEARS), T_CMB0, 1e-6);
    CHECK(cmbTemperature(1e11) < T_CMB0);
    CHECK(std::isfinite(cmbTemperature(1e100)));
    CHECK_NEAR(cmbTemperature(1e100), 0.0, 1e-9);

    if(g_fail){ std::printf("%d checks failed\n", g_fail); return 1; }
    std::printf("all cosmology tests passed\n");
    return 0;
}
