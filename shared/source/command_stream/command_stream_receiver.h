/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/aub_subcapture_status.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/flat_batch_buffer_helper.h"
#include "shared/source/helpers/options.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/kernel/grf_config.h"

#include <cstddef>
#include <cstdint>

namespace NEO {
class AllocationsList;
class Device;
class ExecutionEnvironment;
class ExperimentalCommandBuffer;
class GmmPageTableMngr;
class GraphicsAllocation;
class HostPtrSurface;
class IndirectHeap;
class InternalAllocationStorage;
class LinearStream;
class MemoryManager;
class OsContext;
class OSInterface;
class ScratchSpaceController;
struct HwPerfCounter;
struct HwTimeStamps;
struct TimestampPacketStorage;

template <typename T1>
class TagAllocator;

enum class DispatchMode {
    DeviceDefault = 0,          //default for given device
    ImmediateDispatch,          //everything is submitted to the HW immediately
    AdaptiveDispatch,           //dispatching is handled to async thread, which combines batch buffers basing on load (not implemented)
    BatchedDispatchWithCounter, //dispatching is batched, after n commands there is implicit flush (not implemented)
    BatchedDispatch             // dispatching is batched, explicit clFlush is required
};

class CommandStreamReceiver {
  public:
    enum class SamplerCacheFlushState {
        samplerCacheFlushNotRequired,
        samplerCacheFlushBefore, //add sampler cache flush before Walker with redescribed image
        samplerCacheFlushAfter   //add sampler cache flush after Walker with redescribed image
    };
    using MutexType = std::recursive_mutex;
    CommandStreamReceiver(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
    virtual ~CommandStreamReceiver();

    virtual bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) = 0;

    virtual CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                                      const IndirectHeap &dsh, const IndirectHeap &ioh, const IndirectHeap &ssh,
                                      uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) = 0;

    virtual bool flushBatchedSubmissions() = 0;
    bool submitBatchBuffer(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency);

    MOCKABLE_VIRTUAL void makeResident(GraphicsAllocation &gfxAllocation);
    virtual void makeNonResident(GraphicsAllocation &gfxAllocation);
    MOCKABLE_VIRTUAL void makeSurfacePackNonResident(ResidencyContainer &allocationsForResidency);
    virtual void processResidency(const ResidencyContainer &allocationsForResidency, uint32_t handleId) {}
    virtual void processEviction();
    void makeResidentHostPtrAllocation(GraphicsAllocation *gfxAllocation);

    void ensureCommandBufferAllocation(LinearStream &commandStream, size_t minimumRequiredSize, size_t additionalAllocationSize);

    MemoryManager *getMemoryManager() const;

    ResidencyContainer &getResidencyAllocations();
    ResidencyContainer &getEvictionAllocations();

    virtual GmmPageTableMngr *createPageTableManager() { return nullptr; }
    bool needsPageTableManager(aub_stream::EngineType engineType) const;

    void waitForTaskCountAndCleanAllocationList(uint32_t requiredTaskCount, uint32_t allocationUsage);
    MOCKABLE_VIRTUAL void waitForTaskCountAndCleanTemporaryAllocationList(uint32_t requiredTaskCount);

    LinearStream &getCS(size_t minRequiredSize = 1024u);
    OSInterface *getOSInterface() const;
    ExecutionEnvironment &peekExecutionEnvironment() const { return executionEnvironment; };

    MOCKABLE_VIRTUAL void setTagAllocation(GraphicsAllocation *allocation);
    GraphicsAllocation *getTagAllocation() const {
        return tagAllocation;
    }
    volatile uint32_t *getTagAddress() const { return tagAddress; }

    virtual bool waitForFlushStamp(FlushStamp &flushStampToWait) { return true; };

    uint32_t peekTaskCount() const { return taskCount; }

    uint32_t peekTaskLevel() const { return taskLevel; }
    FlushStamp obtainCurrentFlushStamp() const;

    uint32_t peekLatestSentTaskCount() const { return latestSentTaskCount; }

    uint32_t peekLatestFlushedTaskCount() const { return latestFlushedTaskCount; }

    void enableNTo1SubmissionModel() { this->nTo1SubmissionModelEnabled = true; }
    bool isNTo1SubmissionModelEnabled() const { return this->nTo1SubmissionModelEnabled; }
    void overrideDispatchPolicy(DispatchMode overrideValue) { this->dispatchMode = overrideValue; }

    void setMediaVFEStateDirty(bool dirty) { mediaVfeStateDirty = dirty; }

    void setRequiredScratchSizes(uint32_t newRequiredScratchSize, uint32_t newRequiredPrivateScratchSize);
    GraphicsAllocation *getScratchAllocation();
    GraphicsAllocation *getDebugSurfaceAllocation() const { return debugSurface; }
    GraphicsAllocation *allocateDebugSurface(size_t size);
    GraphicsAllocation *getPreemptionAllocation() const { return preemptionAllocation; }

    void requestStallingPipeControlOnNextFlush() { stallingPipeControlOnNextFlushRequired = true; }
    bool isStallingPipeControlOnNextFlushRequired() const { return stallingPipeControlOnNextFlushRequired; }

    virtual void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool forcePowerSavingMode) = 0;
    MOCKABLE_VIRTUAL bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMicroseconds, uint32_t taskCountToWait);
    virtual void downloadAllocation(GraphicsAllocation &gfxAllocation){};

    void setSamplerCacheFlushRequired(SamplerCacheFlushState value) { this->samplerCacheFlushRequired = value; }

    FlatBatchBufferHelper &getFlatBatchBufferHelper() const { return *flatBatchBufferHelper; }
    void overwriteFlatBatchBufferHelper(FlatBatchBufferHelper *newHelper) { flatBatchBufferHelper.reset(newHelper); }

    MOCKABLE_VIRTUAL void initProgrammingFlags();
    virtual AubSubCaptureStatus checkAndActivateAubSubCapture(const MultiDispatchInfo &dispatchInfo);
    void programForAubSubCapture(bool wasActiveInPreviousEnqueue, bool isActive);
    virtual void addAubComment(const char *comment);

    IndirectHeap &getIndirectHeap(IndirectHeap::Type heapType, size_t minRequiredSize);
    void allocateHeapMemory(IndirectHeap::Type heapType, size_t minRequiredSize, IndirectHeap *&indirectHeap);
    void releaseIndirectHeap(IndirectHeap::Type heapType);

    virtual enum CommandStreamReceiverType getType() = 0;
    void setExperimentalCmdBuffer(std::unique_ptr<ExperimentalCommandBuffer> &&cmdBuffer);

    bool initializeTagAllocation();
    MOCKABLE_VIRTUAL bool createGlobalFenceAllocation();
    MOCKABLE_VIRTUAL bool createPreemptionAllocation();
    MOCKABLE_VIRTUAL bool createPerDssBackedBuffer(Device &device);
    MOCKABLE_VIRTUAL std::unique_lock<MutexType> obtainUniqueOwnership();

    bool peekTimestampPacketWriteEnabled() const { return timestampPacketWriteEnabled; }

    size_t defaultSshSize;

    AllocationsList &getTemporaryAllocations();
    AllocationsList &getAllocationsForReuse();
    InternalAllocationStorage *getInternalAllocationStorage() const { return internalAllocationStorage.get(); }
    MOCKABLE_VIRTUAL bool createAllocationForHostSurface(HostPtrSurface &surface, bool requiresL3Flush);
    virtual size_t getPreferredTagPoolSize() const;
    virtual void setupContext(OsContext &osContext) { this->osContext = &osContext; }
    OsContext &getOsContext() const { return *osContext; }

    TagAllocator<HwTimeStamps> *getEventTsAllocator();
    TagAllocator<HwPerfCounter> *getEventPerfCountAllocator(const uint32_t tagSize);
    TagAllocator<TimestampPacketStorage> *getTimestampPacketAllocator();

    virtual int32_t expectMemory(const void *gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation);

    virtual bool isMultiOsContextCapable() const = 0;

    void setLatestSentTaskCount(uint32_t latestSentTaskCount) {
        this->latestSentTaskCount = latestSentTaskCount;
    }

    virtual uint32_t blitBuffer(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking) = 0;

    ScratchSpaceController *getScratchSpaceController() const {
        return scratchSpaceController.get();
    }

    void registerInstructionCacheFlush() {
        auto mutex = obtainUniqueOwnership();
        requiresInstructionCacheFlush = true;
    }

    bool isLocalMemoryEnabled() const { return localMemoryEnabled; }

    uint32_t getRootDeviceIndex() { return rootDeviceIndex; }

    virtual bool initDirectSubmission(Device &device, OsContext &osContext) {
        return true;
    }

    virtual bool isDirectSubmissionEnabled() const {
        return false;
    }

  protected:
    void cleanupResources();

    std::unique_ptr<FlushStampTracker> flushStamp;
    std::unique_ptr<SubmissionAggregator> submissionAggregator;
    std::unique_ptr<FlatBatchBufferHelper> flatBatchBufferHelper;
    std::unique_ptr<ExperimentalCommandBuffer> experimentalCmdBuffer;
    std::unique_ptr<InternalAllocationStorage> internalAllocationStorage;
    std::unique_ptr<KmdNotifyHelper> kmdNotifyHelper;
    std::unique_ptr<ScratchSpaceController> scratchSpaceController;
    std::unique_ptr<TagAllocator<HwTimeStamps>> profilingTimeStampAllocator;
    std::unique_ptr<TagAllocator<HwPerfCounter>> perfCounterAllocator;
    std::unique_ptr<TagAllocator<TimestampPacketStorage>> timestampPacketAllocator;

    ResidencyContainer residencyAllocations;
    ResidencyContainer evictionAllocations;
    MutexType ownershipMutex;
    ExecutionEnvironment &executionEnvironment;

    LinearStream commandStream;

    volatile uint32_t *tagAddress = nullptr;

    GraphicsAllocation *tagAllocation = nullptr;
    GraphicsAllocation *globalFenceAllocation = nullptr;
    GraphicsAllocation *preemptionAllocation = nullptr;
    GraphicsAllocation *debugSurface = nullptr;
    GraphicsAllocation *perDssBackedBuffer = nullptr;

    IndirectHeap *indirectHeap[IndirectHeap::NUM_TYPES];

    // current taskLevel.  Used for determining if a PIPE_CONTROL is needed.
    std::atomic<uint32_t> taskLevel{0};
    std::atomic<uint32_t> latestSentTaskCount{0};
    std::atomic<uint32_t> latestFlushedTaskCount{0};

    OsContext *osContext = nullptr;
    DispatchMode dispatchMode = DispatchMode::ImmediateDispatch;
    SamplerCacheFlushState samplerCacheFlushRequired = SamplerCacheFlushState::samplerCacheFlushNotRequired;
    PreemptionMode lastPreemptionMode = PreemptionMode::Initial;
    uint64_t totalMemoryUsed = 0u;

    // taskCount - # of tasks submitted
    uint32_t taskCount = 0;
    uint32_t lastSentL3Config = 0;
    uint32_t latestSentStatelessMocsConfig = 0;
    uint32_t lastSentNumGrfRequired = GrfConfig::DefaultGrfNumber;
    uint32_t requiredThreadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
    uint32_t lastSentThreadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
    uint64_t lastSentSliceCount = QueueSliceCount::defaultSliceCount;

    uint32_t requiredScratchSize = 0;
    uint32_t requiredPrivateScratchSize = 0;

    const uint32_t rootDeviceIndex;

    int8_t lastSentCoherencyRequest = -1;
    int8_t lastMediaSamplerConfig = -1;

    bool isPreambleSent = false;
    bool isStateSipSent = false;
    bool isEnginePrologueSent = false;
    bool GSBAFor32BitProgrammed = false;
    bool bindingTableBaseAddressRequired = false;
    bool mediaVfeStateDirty = true;
    bool lastVmeSubslicesConfig = false;
    bool stallingPipeControlOnNextFlushRequired = false;
    bool timestampPacketWriteEnabled = false;
    bool nTo1SubmissionModelEnabled = false;
    bool lastSpecialPipelineSelectMode = false;
    bool requiresInstructionCacheFlush = false;

    bool localMemoryEnabled = false;
    bool pageTableManagerInitialized = false;
};

typedef CommandStreamReceiver *(*CommandStreamReceiverCreateFunc)(bool withAubDump, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex);
} // namespace NEO