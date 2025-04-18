/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Code to notify things that animate before a refresh, at an appropriate
 * refresh rate.  (Perhaps temporary, until replaced by compositor.)
 */

#ifndef nsRefreshDriver_h_
#define nsRefreshDriver_h_

#include "mozilla/FlushType.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "mozilla/WeakPtr.h"
#include "nsTObserverArray.h"
#include "nsTArray.h"
#include "nsTHashSet.h"
#include "nsClassHashtable.h"
#include "nsHashKeys.h"
#include "nsRefreshObservers.h"
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/VisualViewport.h"
#include "mozilla/layers/TransactionIdAllocator.h"
#include "LayersTypes.h"

#include "GeckoProfiler.h"  // for ProfileChunkedBuffer

class nsPresContext;

class imgIRequest;
class nsIRunnable;

struct DocumentFrameCallbacks;

namespace mozilla {
class AnimationEventDispatcher;
class PendingFullscreenEvent;
class PresShell;
class RefreshDriverTimer;
class Runnable;
class Task;
}  // namespace mozilla

class nsRefreshDriver final : public mozilla::layers::TransactionIdAllocator,
                              public nsARefreshObserver {
  using Document = mozilla::dom::Document;
  using TransactionId = mozilla::layers::TransactionId;
  using VVPResizeEvent =
      mozilla::dom::VisualViewport::VisualViewportResizeEvent;
  using VVPScrollEvent =
      mozilla::dom::VisualViewport::VisualViewportScrollEvent;
  using LogPresShellObserver = mozilla::LogPresShellObserver;

 public:
  explicit nsRefreshDriver(nsPresContext* aPresContext);
  ~nsRefreshDriver();

  /**
   * Methods for testing, exposed via nsIDOMWindowUtils.  See
   * nsIDOMWindowUtils.advanceTimeAndRefresh for description.
   */
  void AdvanceTimeAndRefresh(int64_t aMilliseconds);
  void RestoreNormalRefresh();
  void DoTick();
  bool IsTestControllingRefreshesEnabled() const {
    return mTestControllingRefreshes;
  }

  /**
   * Return the time of the most recent refresh.  This is intended to be
   * used by callers who want to start an animation now and want to know
   * what time to consider the start of the animation.  (This helps
   * ensure that multiple animations started during the same event off
   * the main event loop have the same start time.)
   */
  mozilla::TimeStamp MostRecentRefresh(bool aEnsureTimerStarted = true) const;

  /**
   * Add / remove refresh observers.
   * RemoveRefreshObserver returns true if aObserver was found.
   *
   * The flush type affects:
   *   + the order in which the observers are notified (lowest flush
   *     type to highest, in order registered)
   *   + (in the future) which observers are suppressed when the display
   *     doesn't require current position data or isn't currently
   *     painting, and, correspondingly, which get notified when there
   *     is a flush during such suppression
   * and it must be FlushType::Style, or FlushType::Display.
   *
   * The refresh driver does NOT own a reference to these observers;
   * they must remove themselves before they are destroyed.
   *
   * The observer will be called even if there is no other activity.
   */
  void AddRefreshObserver(nsARefreshObserver* aObserver,
                          mozilla::FlushType aFlushType,
                          const char* aObserverDescription);
  bool RemoveRefreshObserver(nsARefreshObserver* aObserver,
                             mozilla::FlushType aFlushType);

  void PostVisualViewportResizeEvent(VVPResizeEvent* aResizeEvent);
  void DispatchVisualViewportResizeEvents();

  void PostScrollEvent(mozilla::Runnable* aScrollEvent, bool aDelayed = false);
  void DispatchScrollEvents();

  MOZ_CAN_RUN_SCRIPT void DispatchResizeEvents();
  MOZ_CAN_RUN_SCRIPT void FlushLayoutOnPendingDocsAndFixUpFocus();

  /**
   * Add an observer that will be called after each refresh. The caller
   * must remove the observer before it is deleted. This does not trigger
   * refresh driver ticks.
   */
  void AddPostRefreshObserver(nsAPostRefreshObserver* aObserver);
  void AddPostRefreshObserver(mozilla::ManagedPostRefreshObserver*) = delete;
  void RemovePostRefreshObserver(nsAPostRefreshObserver* aObserver);
  void RemovePostRefreshObserver(mozilla::ManagedPostRefreshObserver*) = delete;

  /**
   * Add/Remove imgIRequest versions of observers.
   *
   * These are used for hooking into the refresh driver for
   * controlling animated images.
   *
   * @note The refresh driver owns a reference to these listeners.
   *
   * @note Technically, imgIRequest objects are not nsARefreshObservers, but
   * for controlling animated image repaint events, we subscribe the
   * imgIRequests to the nsRefreshDriver for notification of paint events.
   */
  void AddImageRequest(imgIRequest* aRequest);
  void RemoveImageRequest(imgIRequest* aRequest);

  /**
   * Marks that we're currently in the middle of processing user input.
   * Called by EventDispatcher when it's handling an input event.
   */
  void EnterUserInputProcessing() { mUserInputProcessingCount++; }
  void ExitUserInputProcessing() {
    MOZ_ASSERT(mUserInputProcessingCount > 0);
    mUserInputProcessingCount--;
  }

  /**
   * Add / remove presshells which have pending resize event.
   */
  void AddResizeEventFlushObserver(mozilla::PresShell* aPresShell,
                                   bool aDelayed = false) {
    MOZ_DIAGNOSTIC_ASSERT(
        !mResizeEventFlushObservers.Contains(aPresShell) &&
            !mDelayedResizeEventFlushObservers.Contains(aPresShell),
        "Double-adding resize event flush observer");
    if (aDelayed) {
      mDelayedResizeEventFlushObservers.AppendElement(aPresShell);
    } else {
      mResizeEventFlushObservers.AppendElement(aPresShell);
      EnsureTimerStarted();
    }
  }

  void RemoveResizeEventFlushObserver(mozilla::PresShell* aPresShell) {
    mResizeEventFlushObservers.RemoveElement(aPresShell);
    mDelayedResizeEventFlushObservers.RemoveElement(aPresShell);
  }

  void AddStyleFlushObserver(mozilla::PresShell* aPresShell) {
    MOZ_DIAGNOSTIC_ASSERT(!IsStyleFlushObserver(aPresShell),
                          "Double-adding style flush observer");
    LogPresShellObserver::LogDispatch(aPresShell, this);
    mStyleFlushObservers.AppendElement(aPresShell);
    EnsureTimerStarted();
  }
  void RemoveStyleFlushObserver(mozilla::PresShell* aPresShell) {
    mStyleFlushObservers.RemoveElement(aPresShell);
  }
  bool IsStyleFlushObserver(mozilla::PresShell* aPresShell) {
    return mStyleFlushObservers.Contains(aPresShell);
  }

  /**
   * "Early Runner" runnables will be called as the first step when refresh
   * driver tick is triggered. Runners shouldn't keep other objects alive,
   * since it isn't guaranteed they will ever get called.
   */
  void AddEarlyRunner(nsIRunnable* aRunnable) {
    mEarlyRunners.AppendElement(aRunnable);
    EnsureTimerStarted();
  }

  /**
   * Remember whether our presshell's view manager needs a flush
   */
  void ScheduleViewManagerFlush();
  void RevokeViewManagerFlush() { mViewManagerFlushIsPending = false; }
  bool ViewManagerFlushIsPending() { return mViewManagerFlushIsPending; }
  bool HasScheduleFlush() { return mHasScheduleFlush; }
  void ClearHasScheduleFlush() { mHasScheduleFlush = false; }

  /**
   * Queue a new fullscreen event to be dispatched in next tick before
   * the style flush
   */
  void ScheduleFullscreenEvent(
      mozilla::UniquePtr<mozilla::PendingFullscreenEvent> aEvent);

  /**
   * Cancel all pending fullscreen events scheduled by ScheduleFullscreenEvent
   * which targets any node in aDocument.
   */
  void CancelPendingFullscreenEvents(Document* aDocument);

  /**
   * Queue new animation events to dispatch in next tick.
   */
  void ScheduleAnimationEventDispatch(
      mozilla::AnimationEventDispatcher* aDispatcher) {
    NS_ASSERTION(!mAnimationEventFlushObservers.Contains(aDispatcher),
                 "Double-adding animation event flush observer");
    mAnimationEventFlushObservers.AppendElement(aDispatcher);
    EnsureTimerStarted();
  }

  /**
   * Cancel all pending animation events associated with |aDispatcher|.
   */
  void CancelPendingAnimationEvents(
      mozilla::AnimationEventDispatcher* aDispatcher);

  /**
   * Schedule a frame visibility update "soon", subject to the heuristics and
   * throttling we apply to visibility updates.
   */
  void ScheduleFrameVisibilityUpdate() { mNeedToRecomputeVisibility = true; }

  /**
   * Tell the refresh driver that it is done driving refreshes and
   * should stop its timer and forget about its pres context.  This may
   * be called from within a refresh.
   */
  void Disconnect();

  bool IsFrozen() const { return mFreezeCount > 0; }

  bool IsThrottled() const { return mThrottled; }

  /**
   * Freeze the refresh driver.  It should stop delivering future
   * refreshes until thawed. Note that the number of calls to Freeze() must
   * match the number of calls to Thaw() in order for the refresh driver to
   * be un-frozen.
   */
  void Freeze();

  /**
   * Thaw the refresh driver.  If the number of calls to Freeze() matches the
   * number of calls to this function, the refresh driver should start
   * delivering refreshes again.
   */
  void Thaw();

  /**
   * Throttle or unthrottle the refresh driver.  This is done if the
   * corresponding presshell is hidden or shown.
   */
  void SetActivity(bool aIsActive);

  /**
   * Return the prescontext we were initialized with
   */
  nsPresContext* GetPresContext() const;

  void CreateVsyncRefreshTimer();

#ifdef DEBUG
  /**
   * Check whether the given observer is an observer for the given flush type
   */
  bool IsRefreshObserver(nsARefreshObserver* aObserver,
                         mozilla::FlushType aFlushType);
#endif

  /**
   * Default interval the refresh driver uses, in ms.
   */
  static int32_t DefaultInterval();

  /**
   * Returns 1.0 if a recent rate wasn't smaller than
   * DefaultInterval(). Otherwise return rate / DefaultInterval();
   * So the return value is (0-1].
   *
   */
  static double HighRateMultiplier();

  bool IsInRefresh() { return mInRefresh; }

  void SetIsResizeSuppressed() { mResizeSuppressed = true; }
  bool IsResizeSuppressed() const { return mResizeSuppressed; }

  // mozilla::layers::TransactionIdAllocator
  TransactionId GetTransactionId(bool aThrottle) override;
  TransactionId LastTransactionId() const override;
  void NotifyTransactionCompleted(TransactionId aTransactionId) override;
  void RevokeTransactionId(TransactionId aTransactionId) override;
  void ClearPendingTransactions() override;
  void ResetInitialTransactionId(TransactionId aTransactionId) override;
  mozilla::TimeStamp GetTransactionStart() override;
  mozilla::VsyncId GetVsyncId() override;
  mozilla::TimeStamp GetVsyncStart() override;

  bool IsWaitingForPaint(mozilla::TimeStamp aTime);

  void ScheduleAutoFocusFlush(Document* aDocument);

  // nsARefreshObserver
  NS_IMETHOD_(MozExternalRefCountType) AddRef(void) override {
    return TransactionIdAllocator::AddRef();
  }
  NS_IMETHOD_(MozExternalRefCountType) Release(void) override {
    return TransactionIdAllocator::Release();
  }
  virtual void WillRefresh(mozilla::TimeStamp aTime) override;

  /**
   * Compute the time when the currently active refresh driver timer
   * will start its next tick.
   *
   * Expects a non-null default value that is the upper bound of the
   * expected deadline. If the next expected deadline is later than
   * the default value, the default value is returned.
   *
   * If we're animating and we have skipped paints a time in the past
   * is returned.
   *
   * If aCheckType is AllVsyncListeners and we're in the parent process,
   * which doesn't have a RefreshDriver ticking, but some other process does
   * have, the return value is
   * (now + refreshrate - layout.idle_period.time_limit) as an approximation
   * when something will happen.
   * This can be useful check when parent process tries to avoid having too
   * long idle periods for example when it is sending input events to an
   * active child process.
   */
  enum IdleCheck { OnlyThisProcessRefreshDriver, AllVsyncListeners };
  static mozilla::TimeStamp GetIdleDeadlineHint(mozilla::TimeStamp aDefault,
                                                IdleCheck aCheckType);

  /**
   * It returns the expected timestamp of the next tick or nothing if the next
   * tick is missed.
   */
  static mozilla::Maybe<mozilla::TimeStamp> GetNextTickHint();

  static bool IsRegularRateTimerTicking();

  static void DispatchIdleTaskAfterTickUnlessExists(mozilla::Task* aTask);
  static void CancelIdleTask(mozilla::Task* aTask);

  // Schedule a refresh so that any delayed events will run soon.
  void RunDelayedEventsSoon();

  void InitializeTimer() {
    MOZ_ASSERT(!mActiveTimer);
    EnsureTimerStarted();
  }

  bool HasPendingTick() const { return mActiveTimer; }

  void EnsureIntersectionObservationsUpdateHappens() {
    // This is enough to make sure that UpdateIntersectionObservations runs at
    // least once. This is presumably the intent of step 5 in [1]:
    //
    //     Schedule an iteration of the event loop in the root's browsing
    //     context.
    //
    // Though the wording of it is not quite clear to me...
    //
    // [1]:
    // https://w3c.github.io/IntersectionObserver/#dom-intersectionobserver-observe
    EnsureTimerStarted();
    mNeedToUpdateIntersectionObservations = true;
  }

  void EnsureFrameRequestCallbacksHappen() {
    EnsureTimerStarted();
    mNeedToRunFrameRequestCallbacks = true;
  }

  void EnsureResizeObserverUpdateHappens() {
    EnsureTimerStarted();
    mNeedToUpdateResizeObservers = true;
  }

  void EnsureViewTransitionOperationsHappen() {
    EnsureTimerStarted();
    mNeedToUpdateViewTransitions = true;
  }

  void EnsureAnimationUpdate() {
    EnsureTimerStarted();
    mNeedToUpdateAnimations = true;
  }

  void ScheduleMediaQueryListenerUpdate() {
    EnsureTimerStarted();
    mMightNeedMediaQueryListenerUpdate = true;
  }

  void EnsureContentRelevancyUpdateHappens() {
    EnsureTimerStarted();
    mNeedToUpdateContentRelevancy = true;
  }

  // Register a composition payload that will be forwarded to the layer manager
  // if the current or upcoming refresh tick does a paint.
  // If no paint happens, the payload is discarded.
  // Should only be called on root refresh drivers.
  void RegisterCompositionPayload(
      const mozilla::layers::CompositionPayload& aPayload);

  enum class TickReasons : uint32_t {
    eNone = 0,
    eHasObservers = 1 << 0,
    eHasImageRequests = 1 << 1,
    eNeedsToUpdateIntersectionObservations = 1 << 2,
    eNeedsToUpdateContentRelevancy = 1 << 3,
    eHasVisualViewportResizeEvents = 1 << 4,
    eHasScrollEvents = 1 << 5,
    eHasPendingMediaQueryListeners = 1 << 7,
    eNeedsToNotifyResizeObservers = 1 << 8,
    eRootNeedsMoreTicksForUserInput = 1 << 9,
    eNeedsToUpdateAnimations = 1 << 10,
    eNeedsToRunFrameRequestCallbacks = 1 << 11,
    eNeedsToUpdateViewTransitions = 1 << 12,
  };

  void AddForceNotifyContentfulPaintPresContext(nsPresContext* aPresContext);
  void FlushForceNotifyContentfulPaintPresContext();

  // Mark that we've just run a tick from vsync, used to throttle 'extra'
  // paints to one per vsync (see CanDoExtraTick).
  void FinishedVsyncTick() { mAttemptedExtraTickSinceLastVsync = false; }

  void CancelFlushAutoFocus(Document* aDocument);

 private:
  using VisualViewportResizeEventArray = nsTArray<RefPtr<VVPResizeEvent>>;
  using ScrollEventArray = nsTArray<RefPtr<mozilla::Runnable>>;
  using RequestTable = nsTHashSet<RefPtr<imgIRequest>>;
  struct ImageStartData {
    ImageStartData() = default;

    mozilla::Maybe<mozilla::TimeStamp> mStartTime;
    RequestTable mEntries;
  };
  using ImageStartTable = nsClassHashtable<nsUint32HashKey, ImageStartData>;

  struct ObserverData {
    nsARefreshObserver* mObserver;
    const char* mDescription;
    mozilla::TimeStamp mRegisterTime;
    mozilla::MarkerInnerWindowId mInnerWindowId;
    mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> mCause;
    mozilla::FlushType mFlushType;

    bool operator==(nsARefreshObserver* aObserver) const {
      return mObserver == aObserver;
    }
    operator RefPtr<nsARefreshObserver>() { return mObserver; }
  };
  using ObserverArray = nsTObserverArray<ObserverData>;
  MOZ_CAN_RUN_SCRIPT
  void FlushAutoFocusDocuments();
  void RunFullscreenSteps();
  void UpdateAnimationsAndSendEvents();

  MOZ_CAN_RUN_SCRIPT
  void RunVideoAndFrameRequestCallbacks(mozilla::TimeStamp aNowTime);
  MOZ_CAN_RUN_SCRIPT
  void RunVideoFrameCallbacks(const nsTArray<RefPtr<mozilla::dom::Document>>&,
                              mozilla::TimeStamp aNowTime);
  MOZ_CAN_RUN_SCRIPT
  void RunFrameRequestCallbacks(const nsTArray<RefPtr<mozilla::dom::Document>>&,
                                mozilla::TimeStamp aNowTime);
  void UpdateIntersectionObservations(mozilla::TimeStamp aNowTime);
  void UpdateRemoteFrameEffects();
  void UpdateRelevancyOfContentVisibilityAutoFrames();
  MOZ_CAN_RUN_SCRIPT void PerformPendingViewTransitionOperations();
  MOZ_CAN_RUN_SCRIPT void
  DetermineProximityToViewportAndNotifyResizeObservers();
  void MaybeIncreaseMeasuredTicksSinceLoading();
  void EvaluateMediaQueriesAndReportChanges();

  enum class IsExtraTick {
    No,
    Yes,
  };

  // Helper for Tick, to call WillRefresh(aNowTime) on each entry in
  // mObservers[aIdx] and then potentially do some additional post-notification
  // work that's associated with the FlushType corresponding to aIdx.
  //
  // Returns true on success, or false if one of our calls has destroyed our
  // pres context (in which case our callsite Tick() should immediately bail).
  MOZ_CAN_RUN_SCRIPT
  bool TickObserverArray(uint32_t aIdx, mozilla::TimeStamp aNowTime);

  MOZ_CAN_RUN_SCRIPT_BOUNDARY
  void Tick(mozilla::VsyncId aId, mozilla::TimeStamp aNowTime,
            IsExtraTick aIsExtraTick = IsExtraTick::No);

  enum EnsureTimerStartedFlags {
    eNone = 0,
    eForceAdjustTimer = 1 << 0,
    eAllowTimeToGoBackwards = 1 << 1,
    eNeverAdjustTimer = 1 << 2,
  };
  void EnsureTimerStarted(EnsureTimerStartedFlags aFlags = eNone);
  void StopTimer();

  void UpdateThrottledState();

  bool HasObservers() const;
  void AppendObserverDescriptionsToString(nsACString& aStr) const;
  // Note: This should only be called in the dtor of nsRefreshDriver.
  uint32_t ObserverCount() const;
  bool HasImageRequests() const;
  bool ShouldKeepTimerRunningWhileWaitingForFirstContentfulPaint();
  bool ShouldKeepTimerRunningAfterPageLoad();
  ObserverArray& ArrayFor(mozilla::FlushType aFlushType);
  // Trigger a refresh immediately, if haven't been disconnected or frozen.
  void DoRefresh();

  // Starts pending image animations, and refreshes ongoing animations.
  void UpdateAnimatedImages(mozilla::TimeStamp aPreviousRefresh,
                            mozilla::TimeStamp aNowTime);

  bool HasReasonsToTick() const {
    return GetReasonsToTick() != TickReasons::eNone;
  }
  TickReasons GetReasonsToTick() const;
  void AppendTickReasonsToString(TickReasons aReasons, nsACString& aStr) const;

  double GetRegularTimerInterval() const;
  static double GetThrottledTimerInterval();

  static mozilla::TimeDuration GetMinRecomputeVisibilityInterval();

  void FinishedWaitingForTransaction();

  /**
   * Returns true if we didn't tick on the most recent vsync, but we think
   * we could run one now instead in order to reduce latency.
   */
  bool CanDoCatchUpTick();
  /**
   * Returns true if we think it's possible to run an repeat tick (between
   * vsyncs) to hopefully replace the original tick's paint on the compositor.
   * We allow this sometimes for tick requests coming for user input handling
   * to reduce latency.
   */
  bool CanDoExtraTick();

  bool AtPendingTransactionLimit() {
    return mPendingTransactions.Length() == 2;
  }
  bool TooManyPendingTransactions() {
    return mPendingTransactions.Length() >= 2;
  }

  mozilla::RefreshDriverTimer* ChooseTimer();
  mozilla::RefreshDriverTimer* mActiveTimer;
  RefPtr<mozilla::RefreshDriverTimer> mOwnTimer;
  mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> mRefreshTimerStartedCause;

  // nsPresContext passed in constructor and unset in Disconnect.
  mozilla::WeakPtr<nsPresContext> mPresContext;

  RefPtr<nsRefreshDriver> mRootRefresh;

  // The most recently allocated transaction id.
  TransactionId mNextTransactionId;
  AutoTArray<TransactionId, 3> mPendingTransactions;

  uint32_t mFreezeCount;
  uint32_t mUserInputProcessingCount = 0;

  // How long we wait between ticks for throttled (which generally means
  // non-visible) documents registered with a non-throttled refresh driver.
  const mozilla::TimeDuration mThrottledFrameRequestInterval;

  // How long we wait, at a minimum, before recomputing approximate frame
  // visibility information. This is a minimum because, regardless of this
  // interval, we only recompute visibility when we've seen a layout or style
  // flush since the last time we did it.
  const mozilla::TimeDuration mMinRecomputeVisibilityInterval;

  mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> mViewManagerFlushCause;

  bool mThrottled : 1;
  bool mNeedToRecomputeVisibility : 1;
  bool mTestControllingRefreshes : 1;

  // These two fields are almost the same, the only difference is that
  // mViewManagerFlushIsPending gets cleared right before calling
  // ProcessPendingUpdates, and mHasScheduleFlush gets cleared right after
  // calling ProcessPendingUpdates. It is important that mHasScheduleFlush
  // only gets cleared after, but it's not clear if mViewManagerFlushIsPending
  // needs to be cleared before.
  bool mViewManagerFlushIsPending : 1;
  bool mHasScheduleFlush : 1;

  bool mInRefresh : 1;

  // True if the refresh driver is suspended waiting for transaction
  // id's to be returned and shouldn't do any work during Tick().
  bool mWaitingForTransaction : 1;
  // True if Tick() was skipped because of mWaitingForTransaction and
  // we should schedule a new Tick immediately when resumed instead
  // of waiting until the next interval.
  bool mSkippedPaints : 1;

  // True if view managers should delay any resize request until the
  // next tick by the refresh driver. This flag will be reset at the
  // start of every tick.
  bool mResizeSuppressed : 1;

  // True if we need to flush in order to update intersection observations in
  // all our documents.
  bool mNeedToUpdateIntersectionObservations : 1;

  // True if we need to flush in order to update resize observations in all
  // our documents.
  bool mNeedToUpdateResizeObservers : 1;

  // True if we may need to perform pending view transition operations.
  bool mNeedToUpdateViewTransitions : 1;

  // True if we may need to run any frame callback.
  bool mNeedToRunFrameRequestCallbacks : 1;

  // True if we need to update animations.
  bool mNeedToUpdateAnimations : 1;

  // True if we might need to report media query changes in any of our
  // documents.
  bool mMightNeedMediaQueryListenerUpdate : 1;

  // True if we need to update the relevancy of `content-visibility: auto`
  // elements in our documents.
  bool mNeedToUpdateContentRelevancy : 1;

  // True if we're currently within the scope of Tick() handling a normal
  // (timer-driven) tick.
  bool mInNormalTick : 1;

  // True if we attempted an extra tick (see CanDoExtraTick) since the last
  // vsync and thus shouldn't allow another.
  bool mAttemptedExtraTickSinceLastVsync : 1;

  bool mHasExceededAfterLoadTickPeriod : 1;

  bool mHasStartedTimerAtLeastOnce : 1;

  mozilla::TimeStamp mMostRecentRefresh;
  mozilla::TimeStamp mTickStart;
  mozilla::VsyncId mTickVsyncId;
  mozilla::TimeStamp mTickVsyncTime;
  mozilla::TimeStamp mNextThrottledFrameRequestTick;
  mozilla::TimeStamp mNextRecomputeVisibilityTick;
  mozilla::TimeStamp mBeforeFirstContentfulPaintTimerRunningLimit;

  // separate arrays for each flush type we support
  ObserverArray mObservers[3];
  nsTArray<mozilla::layers::CompositionPayload> mCompositionPayloads;
  RequestTable mRequests;
  ImageStartTable mStartTable;
  AutoTArray<nsCOMPtr<nsIRunnable>, 16> mEarlyRunners;
  VisualViewportResizeEventArray mVisualViewportResizeEvents;

  ScrollEventArray mScrollEvents;
  // Scroll events on documents that might have events suppressed.
  ScrollEventArray mDelayedScrollEvents;

  AutoTArray<mozilla::PresShell*, 16> mResizeEventFlushObservers;
  AutoTArray<mozilla::PresShell*, 16> mDelayedResizeEventFlushObservers;
  AutoTArray<mozilla::PresShell*, 16> mStyleFlushObservers;
  nsTArray<RefPtr<Document>> mAutoFocusFlushDocuments;
  nsTObserverArray<nsAPostRefreshObserver*> mPostRefreshObservers;
  nsTArray<mozilla::UniquePtr<mozilla::PendingFullscreenEvent>>
      mPendingFullscreenEvents;
  AutoTArray<mozilla::AnimationEventDispatcher*, 16>
      mAnimationEventFlushObservers;

  // nsPresContexts which `NotifyContentfulPaint` have been called,
  // however the corresponding paint doesn't come from a regular
  // rendering steps(aka tick).
  //
  // For these nsPresContexts, we invoke
  // `FlushForceNotifyContentfulPaintPresContext` in the next tick
  // to force notify contentful paint, regardless whether the tick paints
  // or not.
  nsTArray<mozilla::WeakPtr<nsPresContext>>
      mForceNotifyContentfulPaintPresContexts;

  void BeginRefreshingImages(RequestTable& aEntries,
                             mozilla::TimeStamp aDesired);

  friend class mozilla::RefreshDriverTimer;

  static void Shutdown();
};

#endif /* !defined(nsRefreshDriver_h_) */
