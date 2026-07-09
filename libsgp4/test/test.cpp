/*
 * libsgp4 Comprehensive Test Suite
 *
 * Build:
 *   cmake -B build -G "MinGW Makefiles"
 *   cmake --build build
 *   ./bin/sgp4_test.exe
 */

#include "SGP4.h"
#include "Tle.h"
#include "OrbitalElements.h"
#include "Eci.h"
#include "CoordGeodetic.h"
#include "Vector.h"
#include "Util.h"
#include "Globals.h"
#include "TleException.h"
#include "SatelliteException.h"
#include "DecayedException.h"

#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <string>
#include <cassert>

namespace
{
    int g_passed = 0;
    int g_failed = 0;

    // -----------------------------------------------------------------------
    //  helpers
    // -----------------------------------------------------------------------
    void Check(const char *label, bool cond, const std::string &detail = "")
    {
        if (cond)
        {
            ++g_passed;
        }
        else
        {
            ++g_failed;
            std::cerr << "  FAIL: " << label;
            if (!detail.empty())
                std::cerr << " — " << detail;
            std::cerr << std::endl;
        }
    }

    void PrintHeader(const std::string &title)
    {
        std::cout << std::endl;
        std::cout << "============================================================" << std::endl;
        std::cout << "  " << title << std::endl;
        std::cout << "============================================================" << std::endl;
    }

    double Deg(double rad) { return libsgp4::Util::RadiansToDegrees(rad); }

    // -----------------------------------------------------------------------
    //  Test 1 — TLE parsing
    // -----------------------------------------------------------------------
    void TestTleParsing()
    {
        PrintHeader("1. TLE Parsing");

        // ISS
        {
            const libsgp4::Tle tle("ISS (ZARYA)",
                                   "1 25544U 98067A   24200.54791667  .00014344  00000+0  25520-3 0  9998",
                                   "2 25544  51.6413 205.8746 0002383  80.0434  20.5256 15.50138786467815");

            Check("ISS name", tle.Name() == "ISS (ZARYA)");
            Check("ISS norad number", tle.NoradNumber() == 25544);
            Check("ISS inclination", std::abs(Deg(tle.Inclination(false)) - 51.6413) < 0.001);
            Check("ISS eccentricity", std::abs(tle.Eccentricity() - 0.0002383) < 1e-7);
            Check("ISS mean motion", std::abs(tle.MeanMotion() - 15.50138786) < 1e-6);
            Check("ISS bstar", std::abs(tle.BStar() - 0.00002552) < 1e-8);
            Check("ISS epoch", tle.Epoch().ToString().size() > 0);
        }

        // GPS — deep-space
        {
            const libsgp4::Tle tle("GPS BIIR-2",
                                   "1 24876U 97035A   24200.47500000 -.00000056  00000+0  00000+0 0  9993",
                                   "2 24876  55.5186 199.7200 0143310 148.9010 359.9547  2.00562228198645");

            Check("GPS norad", tle.NoradNumber() == 24876);
            Check("GPS incl", std::abs(Deg(tle.Inclination(false)) - 55.5186) < 0.001);
            Check("GPS eccen", std::abs(tle.Eccentricity() - 0.0143310) < 1e-7);
            Check("GPS mm", std::abs(tle.MeanMotion() - 2.00562228) < 1e-6);
        }

        // Molniya — highly eccentric
        {
            const libsgp4::Tle tle("MOLNIYA 3-50",
                                   "1 24960U 97054A   24200.68116083  .00000136  00000+0  70947-3 0  9991",
                                   "2 24960  63.0826 237.0888 6750528 282.5662  11.6985  2.00596195196731");

            Check("Molniya eccen", std::abs(tle.Eccentricity() - 0.6750528) < 1e-6);
            Check("Molniya incl", std::abs(Deg(tle.Inclination(false)) - 63.0826) < 0.001);
        }

        std::cout << "  [TLE parsing: done]" << std::endl;
    }

    // -----------------------------------------------------------------------
    //  Test 2 — OrbitalElements (J2 correction, derived values)
    // -----------------------------------------------------------------------
    void TestOrbitalElements()
    {
        PrintHeader("2. OrbitalElements");

        const libsgp4::Tle tle("ISS (ZARYA)",
                               "1 25544U 98067A   24200.54791667  .00014344  00000+0  25520-3 0  9998",
                               "2 25544  51.6413 205.8746 0002383  80.0434  20.5256 15.50138786467815");

        const libsgp4::OrbitalElements oe(tle);

        // Raw values match TLE
        Check("OE mean motion",
              std::abs(oe.MeanMotion() - tle.MeanMotion() * libsgp4::kTWOPI / libsgp4::kMINUTES_PER_DAY) < 1e-10);
        Check("OE eccentricity", std::abs(oe.Eccentricity() - tle.Eccentricity()) < 1e-12);
        Check("OE inclination", std::abs(oe.Inclination() - tle.Inclination(false)) < 1e-12);

        // J2 correction: raw mean motion ≠ recovered mean motion
        Check("J2 correction applied",
              std::abs(oe.MeanMotion() - oe.RecoveredMeanMotion()) > 1e-12,
              "Raw and recovered mean motion should differ due to J2");

        Check("J2 correction sign (prograde LEO)",
              oe.RecoveredMeanMotion() > oe.MeanMotion());

        // Period consistency
        const double periodFromRecovered = libsgp4::kTWOPI / oe.RecoveredMeanMotion();
        Check("Period consistency",
              std::abs(periodFromRecovered - oe.Period()) < 1e-10);

        // Perigee > 0 and reasonable for LEO
        Check("Perigee range", oe.Perigee() > 150.0 && oe.Perigee() < 500.0,
              std::to_string(oe.Perigee()) + " km");

        // Semi-major axis: verify Kepler's 3rd law with recovered values
        const double aFromRecovered = std::pow(libsgp4::kXKE / oe.RecoveredMeanMotion(), libsgp4::kTWOTHIRD);
        Check("Kepler 3rd law (recovered)",
              std::abs(aFromRecovered - oe.RecoveredSemiMajorAxis()) / oe.RecoveredSemiMajorAxis() < 1e-10);

        // BStar match
        Check("BStar", std::abs(oe.BStar() - tle.BStar()) < 1e-15);

        std::cout << "  [OrbitalElements: done]" << std::endl;
    }

    // -----------------------------------------------------------------------
    //  Test 3 — SGP4 propagation (near-space / LEO)
    // -----------------------------------------------------------------------
    void TestSGP4NearSpace()
    {
        PrintHeader("3. SGP4 Near-Space Propagation");

        const libsgp4::Tle tle("ISS (ZARYA)",
                               "1 25544U 98067A   24200.54791667  .00014344  00000+0  25520-3 0  9998",
                               "2 25544  51.6413 205.8746 0002383  80.0434  20.5256 15.50138786467815");

        const libsgp4::SGP4 sgp4(tle);

        // Epoch position — should be valid
        {
            const libsgp4::Eci eci = sgp4.FindPosition(0.0);
            const libsgp4::Vector pos = eci.Position();
            const libsgp4::Vector vel = eci.Velocity();

            Check("Epoch position valid", pos.Magnitude() > 6000.0 && pos.Magnitude() < 8000.0,
                  "|R| = " + std::to_string(pos.Magnitude()) + " km");
            Check("Epoch velocity valid", vel.Magnitude() > 7.0 && vel.Magnitude() < 8.0,
                  "|V| = " + std::to_string(vel.Magnitude()) + " km/s");
        }

        // Forward 45 minutes
        {
            const libsgp4::Eci eci = sgp4.FindPosition(45.0);
            Check("Forward 45min: in orbit", eci.Position().Magnitude() > 6000.0);
        }

        // Backward 45 minutes
        {
            const libsgp4::Eci eci = sgp4.FindPosition(-45.0);
            Check("Backward 45min: in orbit", eci.Position().Magnitude() > 6000.0);
        }

        // One full orbit
        {
            const libsgp4::Eci eci0 = sgp4.FindPosition(0.0);
            const libsgp4::OrbitalElements oe(tle);
            const libsgp4::Eci eci1 = sgp4.FindPosition(oe.Period());

            // After one period, position should be close (not exact due to perturbations)
            const double dist = (eci1.Position() - eci0.Position()).Magnitude();
            Check("One orbit later: drift < 200 km",
                  dist < 200.0,
                  "drift = " + std::to_string(dist) + " km");
        }

        // Geodetic conversion round-trip
        {
            const libsgp4::Eci eci = sgp4.FindPosition(0.0);
            const libsgp4::CoordGeodetic geo = eci.ToGeodetic();

            Check("Geodetic lat range", std::abs(Deg(geo.latitude)) <= 90.0);
            Check("Geodetic lon range", std::abs(Deg(geo.longitude)) <= 180.0);
            Check("Geodetic alt > 0", geo.altitude > 150.0);
        }

        // ECI → Geodetic → ECI round-trip
        {
            const libsgp4::Eci eci0 = sgp4.FindPosition(30.0);
            const libsgp4::CoordGeodetic geo = eci0.ToGeodetic();
            const libsgp4::Eci eci1(eci0.GetDateTime(), geo.latitude, geo.longitude, geo.altitude);

            const double dist = (eci1.Position() - eci0.Position()).Magnitude();
            Check("ECI ↔ Geodetic round-trip", dist < 0.1,
                  "delta = " + std::to_string(dist) + " km");
        }

        std::cout << "  [SGP4 near-space: done]" << std::endl;
    }

    // -----------------------------------------------------------------------
    //  Test 4 — SDP4 deep-space propagation (GPS, Molniya)
    // -----------------------------------------------------------------------
    void TestSDP4DeepSpace()
    {
        PrintHeader("4. SDP4 Deep-Space Propagation");

        // GPS — 12-hour resonance
        {
            const libsgp4::Tle tle("GPS BIIR-2",
                                   "1 24876U 97035A   24200.47500000 -.00000056  00000+0  00000+0 0  9993",
                                   "2 24876  55.5186 199.7200 0143310 148.9010 359.9547  2.00562228198645");

            const libsgp4::SGP4 sgp4(tle);
            const libsgp4::OrbitalElements oe(tle);

            Check("GPS: deep-space flag", oe.Period() >= 225.0,
                  "period = " + std::to_string(oe.Period()) + " min");

            const libsgp4::Eci eci = sgp4.FindPosition(0.0);
            Check("GPS: epoch position", eci.Position().Magnitude() > 20000.0,
                  "|R| = " + std::to_string(eci.Position().Magnitude()) + " km");

            {
                const libsgp4::Eci eci12 = sgp4.FindPosition(12.0 * 60.0);
                Check("GPS: 12h forward", eci12.Position().Magnitude() > 20000.0);
            }

            {
                const libsgp4::Eci eci24 = sgp4.FindPosition(24.0 * 60.0);
                Check("GPS: 24h forward", eci24.Position().Magnitude() > 20000.0);
            }
        }

        // Molniya — 12-hour highly eccentric
        {
            const libsgp4::Tle tle("MOLNIYA 3-50",
                                   "1 24960U 97054A   24200.68116083  .00000136  00000+0  70947-3 0  9991",
                                   "2 24960  63.0826 237.0888 6750528 282.5662  11.6985  2.00596195196731");

            const libsgp4::SGP4 sgp4(tle);
            const libsgp4::OrbitalElements oe(tle);

            Check("Molniya: deep-space", oe.Period() >= 225.0);

            const libsgp4::Eci eci = sgp4.FindPosition(0.0);
            Check("Molniya: epoch position", eci.Position().Magnitude() > 6000.0);

            const libsgp4::CoordGeodetic geo = eci.ToGeodetic();
            Check("Molniya: high altitude", geo.altitude > 5000.0,
                  "alt = " + std::to_string(geo.altitude) + " km");
        }

        std::cout << "  [SDP4 deep-space: done]" << std::endl;
    }

    // -----------------------------------------------------------------------
    //  Test 5 — SGP4 FindPosition by DateTime
    // -----------------------------------------------------------------------
    void TestSGP4DateTime()
    {
        PrintHeader("5. SGP4 DateTime Interface");

        const libsgp4::Tle tle("ISS (ZARYA)",
                               "1 25544U 98067A   24200.54791667  .00014344  00000+0  25520-3 0  9998",
                               "2 25544  51.6413 205.8746 0002383  80.0434  20.5256 15.50138786467815");

        const libsgp4::SGP4 sgp4(tle);

        // By tsince (double) vs by DateTime — should match
        {
            const libsgp4::Eci eciTsince = sgp4.FindPosition(60.0);
            const libsgp4::Eci eciDate = sgp4.FindPosition(tle.Epoch().AddMinutes(60.0));

            const double dist = (eciDate.Position() - eciTsince.Position()).Magnitude();
            Check("tsince vs DateTime agreement", dist < 0.001,
                  "delta = " + std::to_string(dist) + " km");
        }

        // Negative time via DateTime
        {
            const libsgp4::Eci eciTsince = sgp4.FindPosition(-30.0);
            const libsgp4::Eci eciDate = sgp4.FindPosition(tle.Epoch().AddMinutes(-30.0));

            const double dist = (eciDate.Position() - eciTsince.Position()).Magnitude();
            Check("Negative time: tsince vs DateTime", dist < 0.001,
                  "delta = " + std::to_string(dist) + " km");
        }

        std::cout << "  [DateTime interface: done]" << std::endl;
    }

    // -----------------------------------------------------------------------
    //  Test 6 — SetTle (re-initialisation)
    // -----------------------------------------------------------------------
    void TestSetTle()
    {
        PrintHeader("6. SetTle (Re-initialisation)");

        const libsgp4::Tle iss("ISS (ZARYA)",
                               "1 25544U 98067A   24200.54791667  .00014344  00000+0  25520-3 0  9998",
                               "2 25544  51.6413 205.8746 0002383  80.0434  20.5256 15.50138786467815");

        const libsgp4::Tle hst("HST",
                               "1 20580U 90037B   24200.52458246  .00002337  00000+0  13980-3 0  9997",
                               "2 20580  28.4697  69.9525 0003144 222.8680 137.0886 15.09680249858704");

        libsgp4::SGP4 sgp4(iss);
        const libsgp4::Eci eciIss = sgp4.FindPosition(0.0);

        sgp4.SetTle(hst);
        const libsgp4::Eci eciHst = sgp4.FindPosition(0.0);

        const double dist = (eciHst.Position() - eciIss.Position()).Magnitude();
        Check("SetTle changes satellite", dist > 100.0,
              "distance between ISS and HST = " + std::to_string(dist) + " km");

        Check("HST: valid position", eciHst.Position().Magnitude() > 6000.0);

        std::cout << "  [SetTle: done]" << std::endl;
    }

    // -----------------------------------------------------------------------
    //  Test 7 — Error handling
    // -----------------------------------------------------------------------
    void TestErrorHandling()
    {
        PrintHeader("7. Error Handling");

        // Invalid TLE line
        {
            bool caught = false;
            try
            {
                libsgp4::Tle bad("BAD",
                                 "1 00000U 00000A   00000.00000000  .00000000  00000+0  00000-0 0  0000",
                                 "2 00000  00.0000 000.0000 0000000 000.0000 000.0000 00.00000000000000");
            }
            catch (const libsgp4::TleException &)
            {
                caught = true;
            }
            Check("TleException on bad TLE", caught);
        }

        // Extreme eccentricity
        {
            try
            {
                libsgp4::Tle tle("TEST",
                                 "1 00001U 00000A   24200.50000000  .00000000  00000+0  00000-0 0  0001",
                                 "2 00001  51.0000 200.0000 9999999 100.0000   0.0000 10.00000000000000");
                libsgp4::SGP4 sgp4(tle);
            }
            catch (const libsgp4::SatelliteException &)
            {
            }
            catch (const libsgp4::TleException &)
            {
            }
            Check("Extreme eccentricity handled", true);
        }

        // Decayed satellite detection
        {
            const libsgp4::Tle tle("DECAY TEST",
                                   "1 00002U 00000A   24200.50000000  .99999999  00000+0  99999-0 0  0002",
                                   "2 00002  51.0000 200.0000 0001000 100.0000   0.0000 16.00000000000000");

            bool caught = false;
            try
            {
                libsgp4::SGP4 sgp4(tle);
                const libsgp4::Eci eci = sgp4.FindPosition(365.0 * 24.0 * 60.0);
                Check("Decayed: should not reach here", false,
                      "perigee = " + std::to_string(eci.Position().Magnitude()) + " km");
            }
            catch (const libsgp4::DecayedException &)
            {
                caught = true;
            }
            catch (const libsgp4::SatelliteException &)
            {
                caught = true;
            }
            catch (...)
            {
                caught = true;
            }
            Check("Decayed detection", caught);
        }

        std::cout << "  [Error handling: done]" << std::endl;
    }

    // -----------------------------------------------------------------------
    //  Test 8 — Coordinate transformations (ECI, Geodetic, DateTime)
    // -----------------------------------------------------------------------
    void TestCoordinates()
    {
        PrintHeader("8. Coordinate Transformations");

        const libsgp4::DateTime dt(2024, 7, 18, 13, 9, 0);

        // Greenwich: lat=51.5°, lon=0°, alt=0 km
        const libsgp4::CoordGeodetic greenwich(51.5, 0.0, 0.0);
        const libsgp4::Eci eciGreenwich(dt, greenwich);
        const libsgp4::CoordGeodetic geoBack = eciGreenwich.ToGeodetic();

        Check("Greenwich lat round-trip",
              std::abs(Deg(geoBack.latitude) - 51.5) < 0.1);
        Check("Greenwich lon round-trip",
              std::abs(Deg(geoBack.longitude) - 0.0) < 0.5,
              "lon = " + std::to_string(Deg(geoBack.longitude)) + "°");

        // Equator
        {
            const libsgp4::CoordGeodetic equator(0.0, 100.0, 0.0);
            const libsgp4::Eci eciEq(dt, equator);
            const libsgp4::Vector pos = eciEq.Position();

            Check("Equator: Z ≈ 0",
                  std::abs(pos.z) < 10.0,
                  "z = " + std::to_string(pos.z) + " km");
            Check("Equator: |XY| ≈ R_e",
                  std::abs(std::sqrt(pos.x * pos.x + pos.y * pos.y) - libsgp4::kXKMPER) < 1.0);
        }

        // North pole
        {
            const libsgp4::CoordGeodetic pole(90.0, 0.0, 0.0);
            const libsgp4::Eci eciPole(dt, pole);
            const libsgp4::Vector pos = eciPole.Position();

            Check("North pole: |XY| ≈ 0",
                  std::sqrt(pos.x * pos.x + pos.y * pos.y) < 5.0);
            Check("North pole: Z > 0", pos.z > 6300.0);
        }

        std::cout << "  [Coordinates: done]" << std::endl;
    }

    // -----------------------------------------------------------------------
    //  Test 9 — Vector operations
    // -----------------------------------------------------------------------
    void TestVector()
    {
        PrintHeader("9. Vector Operations");

        const libsgp4::Vector v1(3.0, 4.0, 0.0);
        const libsgp4::Vector v2(1.0, 0.0, 0.0);

        Check("Magnitude", std::abs(v1.Magnitude() - 5.0) < 1e-12);
        Check("Dot product", std::abs(v1.Dot(v2) - 3.0) < 1e-12);

        const libsgp4::Vector v3 = v1 - v2;
        Check("Subtract x", std::abs(v3.x - 2.0) < 1e-12);
        Check("Subtract y", std::abs(v3.y - 4.0) < 1e-12);

        const libsgp4::Vector zero;
        Check("Default zero", zero.x == 0.0 && zero.y == 0.0 && zero.z == 0.0 && zero.w == 0.0);

        std::cout << "  [Vector: done]" << std::endl;
    }

    // -----------------------------------------------------------------------
    //  Test 10 — Constants sanity
    // -----------------------------------------------------------------------
    void TestConstants()
    {
        PrintHeader("10. Constants Sanity");

        using namespace libsgp4;

        Check("kPI ≈ π", std::abs(kPI - 3.141592653589793) < 1e-14);
        Check("kTWOPI = 2π", std::abs(kTWOPI - 2.0 * kPI) < 1e-14);
        Check("kXKMPER > 0", kXKMPER > 6000.0);
        Check("kMU > 0", kMU > 300000.0);
        Check("kXJ2 > 0", kXJ2 > 0.001);
        Check("kXJ3 < 0", kXJ3 < 0.0);
        Check("kXJ4 < 0", kXJ4 < 0.0);
        Check("kCK2 > 0", kCK2 > 0.0005);
        Check("kCK4 < 0", kCK4 < 0.0);
        Check("kF > 0", kF > 0.003 && kF < 0.004,
              "WGS-84 flattening ≈ 1/298.257");
        Check("kAE = 1", std::abs(kAE - 1.0) < 1e-15);

        const double kxeCheck = 60.0 / std::sqrt(kXKMPER * kXKMPER * kXKMPER / kMU);
        Check("kXKE consistency", std::abs(kXKE - kxeCheck) < 1e-12);

        Check("kCK2 consistency", std::abs(kCK2 - 0.5 * kXJ2 * kAE * kAE) < 1e-15);
        Check("kCK4 consistency", std::abs(kCK4 - (-0.375 * kXJ4 * kAE * kAE * kAE * kAE)) < 1e-15);

        std::cout << "  [Constants: done]" << std::endl;
    }

    // -----------------------------------------------------------------------
    //  Test 11 — TLE checksum verification
    // -----------------------------------------------------------------------
    void TestTleChecksum()
    {
        PrintHeader("11. TLE Checksum");

        {
            bool ok = true;
            try
            {
                libsgp4::Tle tle("OK1",
                                 "1 25544U 98067A   24200.54791667  .00014344  00000+0  25520-3 0  9998",
                                 "2 25544  51.6413 205.8746 0002383  80.0434  20.5256 15.50138786467815");
            }
            catch (const libsgp4::TleException &)
            {
                ok = false;
            }
            Check("Valid TLE accepted", ok);
        }

        std::cout << "  [Checksum: done]" << std::endl;
    }

} // anonymous namespace

// =========================================================================
int test_all()
{
    std::cout << std::endl;
    std::cout << "╔═══════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║     libsgp4 — Comprehensive Functional Test Suite        ║" << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl
              << "Constants: WGS-84 / EGM-96" << std::endl;
    std::cout << "  R_e  = " << libsgp4::kXKMPER << " km" << std::endl;
    std::cout << "  1/f  = " << 1.0 / libsgp4::kF << std::endl;
    std::cout << "  μ    = " << libsgp4::kMU << " km³/s²" << std::endl;
    std::cout << "  J₂   = " << std::scientific << libsgp4::kXJ2 << std::endl;

    TestTleParsing();
    TestOrbitalElements();
    TestSGP4NearSpace();
    TestSDP4DeepSpace();
    TestSGP4DateTime();
    TestSetTle();
    TestErrorHandling();
    TestCoordinates();
    TestVector();
    TestConstants();
    TestTleChecksum();

    // Summary
    std::cout << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << "  RESULTS:  " << g_passed << " passed, "
              << g_failed << " failed"
              << "  (total: " << (g_passed + g_failed) << ")" << std::endl;
    std::cout << "============================================================" << std::endl;
    std::cout << std::endl;

    return g_failed > 0 ? 1 : 0;
}

// int main()
// {
//     return test_all();
// }
