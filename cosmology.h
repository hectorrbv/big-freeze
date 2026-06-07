#pragma once
#include <cmath>
#include <algorithm>

namespace cosmo {

constexpr double C            = 299792458.0;        // m/s
constexpr double G            = 6.67430e-11;        // m^3 kg^-1 s^-2
constexpr double SEC_PER_YEAR = 3.15576e7;          // s/yr
constexpr double MPC_IN_KM    = 3.0856775814913673e19;
constexpr double H0_KM_S_MPC  = 70.0;
constexpr double H0           = (H0_KM_S_MPC / MPC_IN_KM) * SEC_PER_YEAR; // 1/yr
constexpr double OMEGA_M      = 0.315;
constexpr double OMEGA_L      = 0.685;
constexpr double T_CMB0       = 2.725;              // K
constexpr double T0_YEARS     = 13.8e9;            // present age (a normalized to 1 here)
constexpr double LN10         = 2.302585092994046;

// ln(sinh(x)) computed stably for large x (avoids exp overflow)
inline double lnSinh(double x){
    if (x > 20.0) return x - 0.6931471805599453; // ln(0.5 e^x) = x - ln2
    return std::log(std::sinh(x));
}

// ln of the raw (un-normalized) FLRW flat-LCDM scale factor
inline double logScaleFactorRaw(double t_years){
    const double x = 1.5 * std::sqrt(OMEGA_L) * H0 * t_years;
    return (1.0/3.0)*std::log(OMEGA_M/OMEGA_L) + (2.0/3.0)*lnSinh(x);
}

// ln(a), normalized so a(T0_YEARS) = 1
inline double logScaleFactor(double t_years){
    return logScaleFactorRaw(t_years) - logScaleFactorRaw(T0_YEARS);
}

// a(t). May be +inf for astronomically large t -- use logScaleFactor for those.
inline double scaleFactor(double t_years){ return std::exp(logScaleFactor(t_years)); }
inline double log10ScaleFactor(double t_years){ return logScaleFactor(t_years)/LN10; }

// Derived from exp(-logA): underflows to 0 in the far future, never overflows.
inline double redshift(double t_years){ return std::exp(-logScaleFactor(t_years)) - 1.0; }
inline double cmbTemperature(double t_years){ return T_CMB0 * std::exp(-logScaleFactor(t_years)); }

} // namespace cosmo
