#pragma once

#include "ESPressio_PrecisionThread.hpp"

#include <ESPressio_Frequency_Serializable.hpp>
#include <ESPressio_Time_Serializable.hpp>

namespace ESPressio {

    namespace Threads {

        /// <summary>Precision-thread representation policy preserving Serializable Unit wrappers when the iteration time type is serializable.</summary>
        /// <remarks>This specialization is opt-in through this header so the core Threads library remains serialization-agnostic.</remarks>
        template<
            typename TValue,
            Units::UnitOrderOfMagnitude TMagnitude
        >
        struct PrecisionThreadTraits<
            Units::Internal::SerializableUnitType<
                Units::Time<
                    TValue,
                    TMagnitude
                >
            >
        > {
            /// <summary>Serializable public iteration timestamp/delta type.</summary>
            using IterationTime =
                Units::SerializableTime<
                    TValue,
                    TMagnitude
                >;

            /// <summary>Serializable signed nanosecond representation for remaining or overrun time.</summary>
            using SignedIterationTime =
                Units::SerializableTime<
                    int64_t,
                    Units::Nano
                >;

            /// <summary>Serializable representation of measured iteration frequency.</summary>
            using IterationFrequency =
                Units::SerializableFrequency<
                    double
                >;


            /// <summary>Creates the configured serializable signed iteration-time value from nanoseconds.</summary>
            static SignedIterationTime
            CreateSignedIterationTime(
                int64_t nanoseconds
            ) {
                return SignedIterationTime(
                    nanoseconds,
                    Units::Nano
                );
            }


            /// <summary>Creates the configured serializable iteration-frequency value.</summary>
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
