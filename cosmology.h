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

// a(t). Overflows to +inf not long after the present epoch; use logScaleFactor / log10ScaleFactor for large t.
inline double scaleFactor(double t_years){ return std::exp(logScaleFactor(t_years)); }
inline double log10ScaleFactor(double t_years){ return logScaleFactor(t_years)/LN10; }

// Derived from exp(-logA): underflows to 0 in the far future, never overflows.
inline double redshift(double t_years){ return std::exp(-logScaleFactor(t_years)) - 1.0; }
inline double cmbTemperature(double t_years){ return T_CMB0 * std::exp(-logScaleFactor(t_years)); }

struct RGB { float r, g, b; };

// Approximate Planckian-locus color (Tanner Helland approximation). tempK in Kelvin.
inline RGB blackbodyRGB(double tempK){
    double t = std::clamp(tempK, 1000.0, 40000.0) / 100.0;
    double r, g, b;
    if (t <= 66.0) r = 255.0;
    else           r = 329.698727446 * std::pow(t - 60.0, -0.1332047592);
    if (t <= 66.0) g = 99.4708025861 * std::log(t) - 161.1195681661;
    else           g = 288.1221695283 * std::pow(t - 60.0, -0.0755148492);
    if (t >= 66.0)      b = 255.0;
    else if (t <= 19.0) b = 0.0;
    else                b = 138.5177312231 * std::log(t - 10.0) - 305.0447927307;
    auto cl = [](double v){ return (float)(std::clamp(v, 0.0, 255.0) / 255.0); };
    return { cl(r), cl(g), cl(b) };
}

enum class Era { Stelliferous, Degenerate, BlackHole, Dark };
inline Era era(double t_years){
    if (t_years < 1e14)  return Era::Stelliferous;
    if (t_years < 1e40)  return Era::Degenerate;
    if (t_years < 1e100) return Era::BlackHole;
    return Era::Dark;
}
inline const char* eraName(Era e){
    switch(e){
        case Era::Stelliferous: return "Estelifera";
        case Era::Degenerate:   return "Degenerada";
        case Era::BlackHole:    return "Agujeros negros";
        case Era::Dark:         return "Oscura";
    }
    return "?";
}

// --- Stellar lifecycle (CPU mirror of points.vert; keep both in sync) ---
inline double galaxyLuminosity(double t_years, double tForm, double tauSF){
    if (t_years < tForm) return 0.0;
    double age    = t_years - tForm;
    double burst  = std::exp(-age / tauSF);                                  // formation burst fades
    double cutoff = std::clamp(1.0 - (t_years - 1e13)/(1e14 - 1e13), 0.0, 1.0); // dies by ~1e14 yr
    return std::clamp(0.15 + 0.85*burst, 0.0, 1.0) * cutoff;
}
inline double galaxyTempK(double t_years, double tForm, double tauSF){
    double age  = std::max(0.0, t_years - tForm);
    double frac = std::clamp(age / (5.0*tauSF), 0.0, 1.0);
    return 9000.0*(1.0 - frac) + 3000.0*frac; // blue-white -> red as massive stars die
}

// --- Bounded mappings for the renderer (HUD shows true magnitudes) ---
constexpr double GRID_CAP_DECADES = 3.0;
inline double visualStretch(double t_years){
    double d = std::clamp(log10ScaleFactor(t_years), -1.0, GRID_CAP_DECADES);
    return std::pow(10.0, d * 0.25);
}
inline double reddening(double t_years){
    return std::clamp(log10ScaleFactor(t_years) / GRID_CAP_DECADES, 0.0, 1.0);
}

} // namespace cosmo
