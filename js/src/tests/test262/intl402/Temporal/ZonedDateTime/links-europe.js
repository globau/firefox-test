// |reftest| shell-option(--enable-temporal) skip-if(!this.hasOwnProperty('Temporal')||!xulRuntime.shell) -- Temporal is not enabled unconditionally, requires shell-options
// Copyright (C) 2022 André Bargull. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime
description: ZonedDateTime constructor accepts link names as time zone ID input
features: [Temporal, canonical-tz]
---*/

const testCases = [
  "Europe/Jersey",  // Link    Europe/London   Europe/Jersey
  "Europe/Guernsey",  // Link    Europe/London   Europe/Guernsey
  "Europe/Isle_of_Man",  // Link    Europe/London   Europe/Isle_of_Man
  "Europe/Mariehamn",  // Link    Europe/Helsinki Europe/Mariehamn
  "Europe/Busingen",  // Link    Europe/Zurich   Europe/Busingen
  "Europe/Vatican",  // Link    Europe/Rome     Europe/Vatican
  "Europe/San_Marino",  // Link    Europe/Rome     Europe/San_Marino
  "Europe/Vaduz",  // Link Europe/Zurich Europe/Vaduz
  "Arctic/Longyearbyen",  // Link    Europe/Oslo     Arctic/Longyearbyen
  "Europe/Ljubljana",  // Link Europe/Belgrade Europe/Ljubljana   # Slovenia
  "Europe/Podgorica",  // Link Europe/Belgrade Europe/Podgorica   # Montenegro
  "Europe/Sarajevo",  // Link Europe/Belgrade Europe/Sarajevo    # Bosnia and Herzegovina
  "Europe/Skopje",  // Link Europe/Belgrade Europe/Skopje      # North Macedonia
  "Europe/Zagreb",  // Link Europe/Belgrade Europe/Zagreb      # Croatia
  "Europe/Bratislava",  // Link Europe/Prague Europe/Bratislava
  "Asia/Istanbul",  // Link    Europe/Istanbul Asia/Istanbul   # Istanbul is in both continents.
];

for (let id of testCases) {
  const instance = new Temporal.ZonedDateTime(0n, id);
  assert.sameValue(instance.timeZoneId, id);
}

reportCompare(0, 0);
