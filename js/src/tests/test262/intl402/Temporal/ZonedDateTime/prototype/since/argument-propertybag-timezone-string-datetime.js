// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.prototype.since
description: Conversion of ISO date-time strings to Temporal.TimeZone instances (with IANA time zones)
features: [Temporal]
---*/

const expectedTimeZone = "America/Vancouver";
const instance = new Temporal.ZonedDateTime(0n, expectedTimeZone);
let timeZone = "2021-08-19T17:30[America/Vancouver]";
instance.since({ year: 2020, month: 5, day: 2, timeZone });

timeZone = "2021-08-19T17:30Z[America/Vancouver]";
instance.since({ year: 2020, month: 5, day: 2, timeZone });

timeZone = "2021-08-19T17:30-07:00[America/Vancouver]";
instance.since({ year: 2020, month: 5, day: 2, timeZone });

reportCompare(0, 0);
