/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Crash annotation information.

// Generated by `build.rs`.
// static PING_ANNOTATIONS: phf::Set<&'static str>;
// static REPORT_ANNOTATIONS: phf::Set<&'static str>;
include!(concat!(env!("OUT_DIR"), "/crash_annotations.rs"));

/// Return whether the given annotation can be sent in a crash ping.
pub fn send_in_ping(annotation: &str) -> bool {
    PING_ANNOTATIONS.contains(annotation)
}

/// Return whether the given annotation can be sent in a crash report.
pub fn send_in_report(annotation: &str) -> bool {
    REPORT_ANNOTATIONS.contains(annotation) || PING_ANNOTATIONS.contains(annotation)
}
