// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.now.plaintimeiso
description: Time zone IDs are valid input for a time zone
features: [Temporal]
---*/

// The following are all valid strings so should not throw:

["UTC", "+01:00"].forEach((timeZone) => {
  Temporal.Now.plainTimeISO(timeZone);
});

reportCompare(0, 0);
