"use strict";

const { ExperimentAPI, _ExperimentFeature: ExperimentFeature } =
  ChromeUtils.importESModule("resource://nimbus/ExperimentAPI.sys.mjs");
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { NimbusTelemetry } = ChromeUtils.importESModule(
  "resource://nimbus/lib/Telemetry.sys.mjs"
);

async function setupForExperimentFeature() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  await manager.onStartup();

  sandbox.stub(ExperimentAPI, "_manager").get(() => manager);

  return { sandbox, manager };
}

function setDefaultBranch(pref, value) {
  let branch = Services.prefs.getDefaultBranch("");
  branch.setStringPref(pref, value);
}

const TEST_FALLBACK_PREF = "testprefbranch.config";
const FAKE_FEATURE_MANIFEST = {
  description: "Test feature",
  exposureDescription: "Used in tests",
  variables: {
    enabled: {
      type: "boolean",
      fallbackPref: "testprefbranch.enabled",
    },
    config: {
      type: "json",
      fallbackPref: TEST_FALLBACK_PREF,
    },
    remoteValue: {
      type: "boolean",
    },
    test: {
      type: "boolean",
    },
    title: {
      type: "string",
    },
  },
};

/**
 * FOG requires a little setup in order to test it
 */
add_setup(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});

add_task(async function test_ExperimentFeature_test_helper_ready() {
  const { manager } = await setupForExperimentFeature();
  await manager.store.ready();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);

  await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: "foo",
      value: { remoteValue: "mochitest", enabled: true },
    },
    {
      manager,
      isRollout: true,
    }
  );

  Assert.equal(
    featureInstance.getVariable("remoteValue"),
    "mochitest",
    "set by remote config"
  );
});

add_task(async function test_record_exposure_event() {
  const { sandbox, manager } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  const exposureSpy = sandbox.spy(NimbusTelemetry, "recordExposure");
  const getExperimentSpy = sandbox.spy(ExperimentAPI, "getExperimentMetaData");
  sandbox.stub(ExperimentAPI, "_manager").get(() => manager);

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  featureInstance.recordExposureEvent();

  Assert.ok(
    exposureSpy.notCalled,
    "should not emit an exposure event when no experiment is active"
  );

  // Check that there aren't any Glean exposure events yet
  var exposureEvents = Glean.nimbusEvents.exposure.testGetValue("events");
  Assert.equal(
    undefined,
    exposureEvents,
    "no Glean exposure events before exposure"
  );

  await manager.store.addEnrollment(
    ExperimentFakes.experiment("blah", {
      branch: {
        slug: "treatment",
        ratio: 1,
        features: [
          {
            featureId: "foo",
            value: { enabled: false },
          },
        ],
      },
    })
  );

  featureInstance.recordExposureEvent();

  Assert.ok(
    exposureSpy.calledOnce,
    "should emit an exposure event when there is an experiment"
  );
  Assert.equal(getExperimentSpy.callCount, 2, "Should be called every time");

  // Check that the Glean exposure event was recorded.
  exposureEvents = Glean.nimbusEvents.exposure.testGetValue("events");
  // We expect only one event
  Assert.equal(1, exposureEvents.length);
  // And that one event matches the expected
  Assert.equal(
    "blah",
    exposureEvents[0].extra.experiment,
    "Glean.nimbusEvents.exposure recorded with correct experiment slug"
  );
  Assert.equal(
    "treatment",
    exposureEvents[0].extra.branch,
    "Glean.nimbusEvents.exposure recorded with correct branch slug"
  );
  Assert.equal(
    "foo",
    exposureEvents[0].extra.feature_id,
    "Glean.nimbusEvents.exposure recorded with correct feature id"
  );

  sandbox.restore();
});

add_task(async function test_record_exposure_event_once() {
  const { sandbox, manager } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  const exposureSpy = sandbox.spy(NimbusTelemetry, "recordExposure");
  sandbox.stub(ExperimentAPI, "_manager").get(() => manager);

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.store.addEnrollment(
    ExperimentFakes.experiment("blah", {
      branch: {
        slug: "treatment",
        ratio: 1,
        features: [
          {
            featureId: "foo",
            value: { enabled: false },
          },
        ],
      },
    })
  );

  featureInstance.recordExposureEvent({ once: true });
  featureInstance.recordExposureEvent({ once: true });
  featureInstance.recordExposureEvent({ once: true });

  Assert.ok(
    exposureSpy.calledOnce,
    "Should emit a single exposure event when the once param is true."
  );

  // Check that the Glean exposure event was recorded.
  let exposureEvents = Glean.nimbusEvents.exposure.testGetValue("events");
  // We expect only one event
  Assert.equal(1, exposureEvents.length);

  sandbox.restore();
});

add_task(async function test_allow_multiple_exposure_events() {
  const { sandbox, manager } = await setupForExperimentFeature();

  const featureInstance = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  const exposureSpy = sandbox.spy(NimbusTelemetry, "recordExposure");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: "foo",
      value: { enabled: false },
    },
    { manager }
  );

  featureInstance.recordExposureEvent();
  featureInstance.recordExposureEvent();
  featureInstance.recordExposureEvent();

  Assert.ok(exposureSpy.called, "Should emit exposure event");
  Assert.equal(
    exposureSpy.callCount,
    3,
    "Should emit an exposure event for each function call"
  );

  // Check that the Glean exposure event was recorded.
  let exposureEvents = Glean.nimbusEvents.exposure.testGetValue("events");
  // We expect 3 events
  Assert.equal(3, exposureEvents.length);

  sandbox.restore();
  doExperimentCleanup();
});

add_task(async function test_onUpdate_before_store_ready() {
  let sandbox = sinon.createSandbox();
  const feature = new ExperimentFeature("foo", FAKE_FEATURE_MANIFEST);
  const stub = sandbox.stub();
  const manager = ExperimentFakes.manager();
  sandbox.stub(ExperimentAPI, "_manager").get(() => manager);
  sandbox.stub(manager.store, "getAllActiveExperiments").returns([
    ExperimentFakes.experiment("foo-experiment", {
      branch: {
        slug: "control",
        features: [
          {
            featureId: "foo",
            value: null,
          },
        ],
      },
    }),
  ]);

  // We register for updates before the store finished loading experiments
  // from disk
  feature.onUpdate(stub);

  await manager.onStartup();

  Assert.ok(
    stub.calledOnce,
    "Called on startup after loading experiments from disk"
  );
  Assert.equal(
    stub.firstCall.args[0],
    `featureUpdate:${feature.featureId}`,
    "Called for correct feature"
  );

  Assert.equal(
    stub.firstCall.args[1],
    "feature-experiment-loaded",
    "Called for the expected reason"
  );
});

add_task(async function test_ExperimentFeature_test_ready_late() {
  const { manager, sandbox } = await setupForExperimentFeature();
  const stub = sandbox.stub();

  const featureInstance = new ExperimentFeature(
    "test-feature",
    FAKE_FEATURE_MANIFEST
  );

  const rollout = ExperimentFakes.rollout("foo", {
    branch: {
      slug: "slug",
      features: [
        {
          featureId: featureInstance.featureId,
          value: {
            title: "hello",
            enabled: true,
          },
        },
      ],
    },
  });

  sandbox.stub(manager.store, "getAllActiveRollouts").returns([rollout]);

  await manager.onStartup();
  await manager.store.ready();

  featureInstance.onUpdate(stub);

  Assert.ok(stub.calledOnce, "Callback called");
  Assert.equal(stub.firstCall.args[0], "featureUpdate:test-feature");
  Assert.equal(stub.firstCall.args[1], "rollout-updated");

  setDefaultBranch(TEST_FALLBACK_PREF, JSON.stringify({ foo: true }));

  Assert.deepEqual(
    featureInstance.getVariable("config"),
    { foo: true },
    "Feature is ready even when initialized after store update"
  );
  Assert.equal(
    featureInstance.getVariable("title"),
    "hello",
    "Returns the NimbusTestUtils rollout default value"
  );
});
