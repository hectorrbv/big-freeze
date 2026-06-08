#include <cstdio>
#include <cmath>
#include <string>
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

    CHECK(era(T0_YEARS)  == Era::Stelliferous);
    CHECK(era(1e13)      == Era::Stelliferous);
    CHECK(era(1e20)      == Era::Degenerate);
    CHECK(era(1e60)      == Era::BlackHole);
    CHECK(era(1e110)     == Era::Dark);
    CHECK(std::string(eraName(Era::BlackHole)) == "Agujeros negros");

    {
        RGB hot  = blackbodyRGB(40000.0); // very hot -> blue dominates
        RGB cold = blackbodyRGB(1000.0);  // cool -> red dominates
        CHECK(hot.b  > hot.r);
        CHECK(cold.r > cold.b);
        CHECK(cold.r > 0.9f);             // near-saturated red
        // channels stay in [0,1]
        CHECK(hot.r >= 0.0f && hot.r <= 1.0f);
        CHECK(hot.b >= 0.0f && hot.b <= 1.0f);
    }

    {
        double tForm = 5e8, tau = 3e9;
        // not yet formed
        CHECK_NEAR(galaxyLuminosity(1e8, tForm, tau), 0.0, 1e-9);
        // bright while young, dimmer when old (within stelliferous era)
        CHECK(galaxyLuminosity(1e9, tForm, tau) > galaxyLuminosity(1e12, tForm, tau));
        // effectively dark after the stelliferous era
        CHECK(galaxyLuminosity(2e14, tForm, tau) < 1e-6);
        // bluer (hotter) when young than when old
        CHECK(galaxyTempK(1e9, tForm, tau) > galaxyTempK(8e9, tForm, tau));
    }
    {
        // bounded render mappings
        CHECK(visualStretch(T0_YEARS) >= 1.0);
        CHECK(std::isfinite(visualStretch(1e100)));
        CHECK(reddening(T0_YEARS) >= 0.0 && reddening(T0_YEARS) <= 1.0);
        CHECK_NEAR(reddening(1e100), 1.0, 1e-9);
    }

    // entropy proxy: 0 early, 1 at heat death, monotonic in log time
    CHECK(entropyFraction(1e8) < 0.05);
    CHECK_NEAR(entropyFraction(1e100), 1.0, 1e-9);
    CHECK(entropyFraction(1e20) > entropyFraction(1e12));
    CHECK(entropyFraction(1e8) >= 0.0 && entropyFraction(1e100) <= 1.0);

    if(g_fail){ std::printf("%d checks failed\n", g_fail); return 1; }
    std::printf("all cosmology tests passed\n");
    return 0;
}
