/*
 * Copyright 2013 Daniel Warner <contact@danrw.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cmath>

namespace libsgp4
{

    /*
     * --------------------------------------------------------------------------
     * 大气阻力模型 (Static Non-Rotating Spherical Atmosphere, Spacetrack Report #3)
     *
     * SGP4 使用幂函数大气密度模型 ρ ∝ (q₀ - s)⁴，以下三个参数定义了该密度剖面。
     * kAE 和 kS0 共同决定大气密度标高处的地心距 S = kAE * (1 + kS0/kXKMPER) ≈ 1.012，
     * 该值在全部近地系数 (c1, c4, d2-d4, t3cof-t5cof) 中起关键作用。
     * --------------------------------------------------------------------------
     */

    const double kAE = 1.0;   // 等效大气高度 (地球半径单位)，密度标高归一化参考值
    const double kQ0 = 120.0; // 大气密度模型上界高度 (km)
    const double kS0 = 78.0;  // 大气密度模型下界高度 (km)，该处密度达到模型最大值

    /*
     * --------------------------------------------------------------------------
     * 地球引力场基本参数 (WGS-84 / EGM-96)
     *
     * 对应 SGP4_vallado getgravconst() 的 wgs84 情形。
     * J2 是 SGP4 最主要的扰动源，引起 Ω 和 ω 的长期进动。
     * --------------------------------------------------------------------------
     */

    const double kMU = 398600.5;          // 地心引力常数 (km³/s²)，决定轨道运动的基本尺度
    const double kXKMPER = 6378.137;      // 地球赤道半径 (km)，最终输出时用于将无量纲量转为公里
    const double kXJ2 = 1.08262998905e-3; // J2 带谐系数 (扁率)，量级 10⁻³，SGP4 最主要的非球形引力扰动
    const double kXJ3 = -2.53215306e-6;   // J3 带谐系数 (南北不对称)，为负表示南半球质量略大
    const double kXJ4 = -1.61098761e-6;   // J4 带谐系数 (高阶扁率修正)，对 J2 效应提供精细修正

    // -- 由上述基本参数派生的常数 --

    // 归一化地球引力系数: 60/√(R_e³/μ)，在平运动 n (rad/min) 与半长轴 a (地球半径单位) 间转换
    // a = (kXKE / n)^(2/3)，n = kXKE / a^(1.5)
    // 备选值: dundee 预计算为 7.43669161331734132e-2
    const double kXKE = 60.0 / sqrt(kXKMPER * kXKMPER * kXKMPER / kMU);

    const double kCK2 = 0.5 * kXJ2 * kAE * kAE;                // J2 归一化系数: ½·J2·AE²
    const double kCK4 = -0.375 * kXJ4 * kAE * kAE * kAE * kAE; // J4 归一化系数: -⅜·J4·AE⁴

    // 大气密度缩放因子: ((Q₀-S₀)/R_e)⁴ ≈ 1.88×10⁻⁹，影响 B* 阻力对半长轴的衰减速率
    // 备选值: dundee 预计算为 1.880279159015270643865e-9
    const double kQOMS2T = pow(((kQ0 - kS0) / kXKMPER), 4.0);

    const double kS = kAE * (1.0 + kS0 / kXKMPER); // 大气密度标高处的归一化地心距 ≈ 1.012

    // -- 数学常数 --

    const double kPI = 3.14159265358979323846264338327950288419716939937510582; // π (80位)
    const double kTWOPI = 2.0 * kPI;                                            // 2π，用于角度归化到 [0, 2π)
    const double kTWOTHIRD = 2.0 / 3.0;                                         // 2/3，开普勒第三定律指数: a ∝ n^(-2/3)

    // -- 时间与地球自转常数 --

    // 地球每恒星日自转角度 (rad/min)，深空共振判断关键: 卫星平运动与之匹配时触发 24h 共振积分器
    const double kTHDT = 4.37526908801129966e-3;

    const double kF = 1.0 / 298.257223563;  // 地球扁率因子 (WGS-84)，用于大地坐标 ↔ ECI 转换
    const double kOMEGA_E = 1.00273790934; // 恒星日/太阳日比值，GMST 计算中使用

    // 1 天文单位 (km)，IAU 2012 Resolution B2，用于 SolarPosition 计算太阳 ECI 位置
    const double kAU = 1.49597870700e8;

    const double kSECONDS_PER_DAY = 86400.0; // 每日秒数
    const double kMINUTES_PER_DAY = 1440.0;  // 每日分钟数
    const double kHOURS_PER_DAY = 24.0;      // 每日小时数

    // J3/J2 组合系数: -J3/CK2 × AE³，用于计算长周期项振幅 xlcof 和 aycof:
    //   xlcof = 0.125 × A3OVK2 × sin(i) × (3+5cos(i)) / (1+cos(i))
    //   aycof = 0.25  × A3OVK2 × sin(i)
    const double kA3OVK2 = -kXJ3 / kCK2 * kAE * kAE * kAE;

} // namespace libsgp4
