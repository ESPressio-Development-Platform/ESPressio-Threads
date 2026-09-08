#pragma once

#include <cstdint>

#include "ESPressio_IObserver.hpp"
#include "ESPressio_ClockTypes.hpp"
#include "ESPressio_PrecisionThreadTraits.hpp"

namespace ESPressio {

    namespace Threads {

        template<
            typename TTime = Timing::DefaultClockTime,
            typename TRepresentationTraits =
                PrecisionThreadTraits<TTime>
        >
        class PrecisionThread;

        /// <summary>Number of periodic iterations skipped when a precision thread falls behind its schedule.</summary>
        using SkippedIterationCount =
            uint64_t;


        /// <summary>Observer contract for completed precision-thread iterations.</summary>
/**
 * ESPressio Memory Audit
 * Inherited Memory Total: sizeof(Observable::IObserver) [0 bytes dynamic allocation]
 * Members: none; polymorphic interface/object includes vptr storage where not supplied by a base.
 * Total Memory: sizeof(Observable::IObserver) + 4 bytes vptr [0 bytes dynamic allocation]
 * Basis: ESP32/Xtensa ILP32 reference ABI; GNU libstdc++ container control-block sizes are implementation-sensitive.
 * Confidence: low; compile-time sizeof on the concrete target remains authoritative for ABI-sensitive/opaque members.
 * End ESPressio Memory Audit
 */
template<
            typename TTime = Timing::DefaultClockTime,
            typename TRepresentationTraits =
                PrecisionThreadTraits<TTime>
        >
        class IPrecisionThreadObserver :
            public virtual Observable::IObserver {

            public:
                using TimeType =
                    TTime;

                using RepresentationTraits =
                    TRepresentationTraits;

                using ThreadType =
                    PrecisionThread<
                        TTime,
                        TRepresentationTraits
                    >;

                virtual ~IPrecisionThreadObserver() =
                    default;


                /// <summary>Called after one scheduled iteration, including scheduled/actual timing and skipped-iteration count.</summary>
                virtual void
                OnPrecisionThreadIteration(
                    ThreadType*,
                    TTime,
                    TTime,
                    SkippedIterationCount
                ) {
                }
        };

    }

}
