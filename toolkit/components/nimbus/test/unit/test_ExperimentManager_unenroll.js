"use strict";

const { TelemetryEvents } = ChromeUtils.importESModule(
  "resource://normandy/lib/TelemetryEvents.sys.mjs"
);
const { TelemetryEnvironment } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryEnvironment.sys.mjs"
);
const STUDIES_OPT_OUT_PREF = "app.shield.optoutstudies.enabled";
const UPLOAD_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";

/**
 * FOG requires a little setup in order to test it
 */
add_setup(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});

/**
 * Normal unenrollment for experiments:
 * - set .active to false
 * - set experiment inactive in telemetry
 * - send unrollment event
 */
add_task(async function test_set_inactive() {
  const manager = ExperimentFakes.manager();

  await manager.onStartup();
  await manager.store.addEnrollment(ExperimentFakes.experiment("foo"));

  manager.unenroll("foo");

  Assert.equal(
    manager.store.get("foo").active,
    false,
    "should set .active to false"
  );

  assertEmptyStore(manager.store);
});

add_task(async function test_unenroll_opt_out() {
  const sandbox = sinon.createSandbox();
  sandbox.spy(TelemetryEvents, "sendEvent");

  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, true);
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(experiment);

  // Check that there aren't any Glean unenrollment events yet
  var unenrollmentEvents =
    Glean.nimbusEvents.unenrollment.testGetValue("events");
  Assert.equal(
    undefined,
    unenrollmentEvents,
    "no Glean unenrollment events before unenrollment"
  );

  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, false);

  Assert.equal(
    manager.store.get(experiment.slug).active,
    false,
    "should set .active to false"
  );
  Assert.ok(TelemetryEvents.sendEvent.calledOnce);
  Assert.deepEqual(
    TelemetryEvents.sendEvent.firstCall.args,
    [
      "unenroll",
      "nimbus_experiment",
      experiment.slug,
      {
        reason: "studies-opt-out",
        branch: experiment.branch.slug,
      },
    ],
    "should send an unenrollment ping with the slug, reason, and branch slug"
  );

  // Check that the Glean unenrollment event was recorded.
  unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue("events");
  // We expect only one event
  Assert.equal(1, unenrollmentEvents.length);
  // And that one event matches the expected enrolled experiment
  Assert.equal(
    experiment.slug,
    unenrollmentEvents[0].extra.experiment,
    "Glean.nimbusEvents.unenrollment recorded with correct experiment slug"
  );
  Assert.equal(
    experiment.branch.slug,
    unenrollmentEvents[0].extra.branch,
    "Glean.nimbusEvents.unenrollment recorded with correct branch slug"
  );
  Assert.equal(
    "studies-opt-out",
    unenrollmentEvents[0].extra.reason,
    "Glean.nimbusEvents.unenrollment recorded with correct reason"
  );

  assertEmptyStore(manager.store);

  // reset pref
  Services.prefs.clearUserPref(STUDIES_OPT_OUT_PREF);
  sandbox.restore();
});

add_task(async function test_unenroll_rollout_opt_out() {
  const sandbox = sinon.createSandbox();
  sandbox.spy(TelemetryEvents, "sendEvent");

  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, true);
  const manager = ExperimentFakes.manager();
  const rollout = ExperimentFakes.rollout("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(rollout);

  // Check that there aren't any Glean unenrollment events yet
  var unenrollmentEvents =
    Glean.nimbusEvents.unenrollment.testGetValue("events");
  Assert.equal(
    undefined,
    unenrollmentEvents,
    "no Glean unenrollment events before unenrollment"
  );

  Services.prefs.setBoolPref(STUDIES_OPT_OUT_PREF, false);

  Assert.equal(
    manager.store.get(rollout.slug).active,
    false,
    "should set .active to false"
  );
  Assert.ok(TelemetryEvents.sendEvent.calledOnce);
  Assert.deepEqual(
    TelemetryEvents.sendEvent.firstCall.args,
    [
      "unenroll",
      "nimbus_experiment",
      rollout.slug,
      {
        reason: "studies-opt-out",
        branch: rollout.branch.slug,
      },
    ],
    "should send an unenrollment ping with the slug, reason, and branch slug"
  );

  // Check that the Glean unenrollment event was recorded.
  unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue("events");
  // We expect only one event
  Assert.equal(1, unenrollmentEvents.length);
  // And that one event matches the expected enrolled experiment
  Assert.equal(
    rollout.slug,
    unenrollmentEvents[0].extra.experiment,
    "Glean.nimbusEvents.unenrollment recorded with correct rollout slug"
  );
  Assert.equal(
    rollout.branch.slug,
    unenrollmentEvents[0].extra.branch,
    "Glean.nimbusEvents.unenrollment recorded with correct branch slug"
  );
  Assert.equal(
    "studies-opt-out",
    unenrollmentEvents[0].extra.reason,
    "Glean.nimbusEvents.unenrollment recorded with correct reason"
  );

  assertEmptyStore(manager.store);

  // reset pref
  Services.prefs.clearUserPref(STUDIES_OPT_OUT_PREF);
  sandbox.restore();
});

add_task(async function test_unenroll_uploadPref() {
  const manager = ExperimentFakes.manager();
  const recipe = ExperimentFakes.recipe("foo");

  await manager.onStartup();
  await ExperimentFakes.enrollmentHelper(recipe, { manager });

  Assert.equal(
    manager.store.get(recipe.slug).active,
    true,
    "Should set .active to true"
  );

  Services.prefs.setBoolPref(UPLOAD_ENABLED_PREF, false);

  Assert.equal(
    manager.store.get(recipe.slug).active,
    false,
    "Should set .active to false"
  );

  assertEmptyStore(manager.store);

  Services.prefs.clearUserPref(UPLOAD_ENABLED_PREF);
});

add_task(async function test_setExperimentInactive_called() {
  const sandbox = sinon.createSandbox();
  sandbox.spy(TelemetryEnvironment, "setExperimentInactive");

  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.recipe("foo", {
    bucketConfig: {
      ...ExperimentFakes.recipe.bucketConfig,
      count: 1000,
    },
  });

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.enroll(experiment);

  // Test Glean experiment API interaction
  Assert.notEqual(
    undefined,
    Services.fog.testGetExperimentData(experiment.slug),
    "experiment should be active before unenroll"
  );

  manager.unenroll("foo");

  Assert.ok(
    TelemetryEnvironment.setExperimentInactive.calledWith("foo"),
    "should call TelemetryEnvironment.setExperimentInactive with slug"
  );

  // Test Glean experiment API interaction
  Assert.equal(
    undefined,
    Services.fog.testGetExperimentData(experiment.slug),
    "experiment should be inactive after unenroll"
  );

  assertEmptyStore(manager.store);

  sandbox.restore();
});

add_task(async function test_send_unenroll_event() {
  const sandbox = sinon.createSandbox();
  sandbox.spy(TelemetryEvents, "sendEvent");

  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(experiment);

  // Check that there aren't any Glean unenrollment events yet
  var unenrollmentEvents =
    Glean.nimbusEvents.unenrollment.testGetValue("events");
  Assert.equal(
    undefined,
    unenrollmentEvents,
    "no Glean unenrollment events before unenrollment"
  );

  manager.unenroll("foo", { reason: "some-reason" });

  Assert.ok(TelemetryEvents.sendEvent.calledOnce);
  Assert.deepEqual(
    TelemetryEvents.sendEvent.firstCall.args,
    [
      "unenroll",
      "nimbus_experiment",
      "foo", // slug
      {
        reason: "some-reason",
        branch: experiment.branch.slug,
      },
    ],
    "should send an unenrollment ping with the slug, reason, and branch slug"
  );

  // Check that the Glean unenrollment event was recorded.
  unenrollmentEvents = Glean.nimbusEvents.unenrollment.testGetValue("events");
  // We expect only one event
  Assert.equal(1, unenrollmentEvents.length);
  // And that one event matches the expected enrolled experiment
  Assert.equal(
    experiment.slug,
    unenrollmentEvents[0].extra.experiment,
    "Glean.nimbusEvents.unenrollment recorded with correct experiment slug"
  );
  Assert.equal(
    experiment.branch.slug,
    unenrollmentEvents[0].extra.branch,
    "Glean.nimbusEvents.unenrollment recorded with correct branch slug"
  );
  Assert.equal(
    "some-reason",
    unenrollmentEvents[0].extra.reason,
    "Glean.nimbusEvents.unenrollment recorded with correct reason"
  );

  assertEmptyStore(manager.store);

  sandbox.restore();
});

add_task(async function test_undefined_reason() {
  const sandbox = sinon.createSandbox();
  sandbox.spy(TelemetryEvents, "sendEvent");

  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("foo");

  // Clear any pre-existing data in Glean
  Services.fog.testResetFOG();

  await manager.onStartup();
  await manager.store.addEnrollment(experiment);

  manager.unenroll("foo");

  const options = TelemetryEvents.sendEvent.firstCall?.args[3];
  Assert.ok(
    "reason" in options,
    "options object with .reason should be the fourth param"
  );
  Assert.equal(
    options.reason,
    "unknown",
    "should include unknown as the reason if none was supplied"
  );

  // Check that the Glean unenrollment event was recorded.
  let unenrollmentEvents =
    Glean.nimbusEvents.unenrollment.testGetValue("events");
  // We expect only one event
  Assert.equal(1, unenrollmentEvents.length);
  // And that one event reason matches the expected reason
  Assert.equal(
    "unknown",
    unenrollmentEvents[0].extra.reason,
    "Glean.nimbusEvents.unenrollment recorded with correct (unknown) reason"
  );

  assertEmptyStore(manager.store);

  sandbox.restore();
});

/**
 * Normal unenrollment for rollouts:
 * - remove stored enrollment and synced data (prefs)
 * - set rollout inactive in telemetry
 * - send unrollment event
 */

add_task(async function test_remove_rollouts() {
  const store = ExperimentFakes.store();
  const manager = ExperimentFakes.manager(store);
  const rollout = ExperimentFakes.rollout("foo");

  sinon.stub(store, "get").returns(rollout);
  sinon.spy(store, "updateExperiment");

  await manager.onStartup();

  manager.unenroll("foo", { reason: "some-reason" });

  Assert.ok(
    manager.store.updateExperiment.calledOnce,
    "Called to set the rollout as !active"
  );
  Assert.ok(
    manager.store.updateExperiment.calledWith(rollout.slug, {
      active: false,
      unenrollReason: "some-reason",
    }),
    "Called with expected parameters"
  );

  assertEmptyStore(manager.store);
});

add_task(async function test_unenroll_individualOptOut_statusTelemetry() {
  Services.fog.testResetFOG();

  const manager = ExperimentFakes.manager();

  await manager.onStartup();

  await manager.enroll(
    ExperimentFakes.recipe("foo", {
      bucketConfig: {
        ...ExperimentFakes.recipe.bucketConfig,
        count: 1000,
      },
      branches: [ExperimentFakes.recipe.branches[0]],
    })
  );

  Services.fog.applyServerKnobsConfig(
    JSON.stringify({
      metrics_enabled: {
        "nimbus_events.enrollment_status": true,
      },
    })
  );

  manager.unenroll("foo", { reason: "individual-opt-out" });

  Assert.deepEqual(
    Glean.nimbusEvents.enrollmentStatus
      .testGetValue("events")
      ?.map(ev => ev.extra),
    [
      {
        slug: "foo",
        branch: "control",
        status: "Disqualified",
        reason: "OptOut",
      },
    ]
  );
});
