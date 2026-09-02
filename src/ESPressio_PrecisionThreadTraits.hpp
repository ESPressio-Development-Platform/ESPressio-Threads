#pragma once

#include <cstdint>

#include "ESPressio_Frequency.hpp"
#include "ESPressio_Time.hpp"

namespace ESPressio {

    namespace Threads {

        /// <summary>Public Unit representation policy used by <c>PrecisionThread</c> for iteration time, signed overrun/remaining time, and measured frequency.</summary>
        /// <typeparam name="TTime">Public time representation used for iteration timestamps and deltas.</typeparam>
        /// <remarks>The default policy uses ordinary ESPressio Units and introduces no Serializable dependency.</remarks>
        template<typename TTime>
        struct PrecisionThreadTraits {
            /// <summary>Public iteration timestamp/delta type.</summary>
            using IterationTime = TTime;

            /// <summary>Signed nanosecond representation used for remaining or overrun time.</summary>
            using SignedIterationTime =
                Units::Time<
                    int64_t,
                    Units::Nano
                >;

            /// <summary>Public frequency representation used for measured iteration rate.</summary>
            using IterationFrequency =
                Units::Frequency<double>;


            /// <summary>Creates the configured signed iteration-time representation from nanoseconds.</summary>
            static SignedIterationTime
            CreateSignedIterationTime(
                int64_t nanoseconds
            ) {
                return SignedIterationTime(
                    nanoseconds,
                    Units::Nano
                );
            }


            /// <summary>Creates the configured iteration-frequency representation.</summary>
            static IterationFrequency
            CreateIterationFrequency(
                double frequency
            ) {
                return IterationFrequency(
                    frequency
                );
            }
        };

    }

}
