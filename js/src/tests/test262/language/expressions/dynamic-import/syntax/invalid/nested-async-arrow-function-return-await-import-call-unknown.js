// |reftest| skip error:SyntaxError -- source-phase-imports is not supported
// This file was procedurally generated from the following sources:
// - src/dynamic-import/import-call-unknown.case
// - src/dynamic-import/syntax/invalid/nested-async-arrow-fn-return-await.template
/*---
description: It's a SyntaxError on unknown import call (nested in async arrow function, returned)
esid: sec-import-call-runtime-semantics-evaluation
features: [source-phase-imports, dynamic-import]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    ImportCall :
        import( AssignmentExpression )

    1. Let referencingScriptOrModule be ! GetActiveScriptOrModule().
    2. Assert: referencingScriptOrModule is a Script Record or Module Record (i.e. is not null).
    3. Let argRef be the result of evaluating AssignmentExpression.
    4. Let specifier be ? GetValue(argRef).
    5. Let promiseCapability be ! NewPromiseCapability(%Promise%).
    6. Let specifierString be ToString(specifier).
    7. IfAbruptRejectPromise(specifierString, promiseCapability).
    8. Perform ! HostImportModuleDynamically(referencingScriptOrModule, specifierString, promiseCapability).
    9. Return promiseCapability.[[Promise]].


    ImportCall[Yield, Await] :
        import ( AssignmentExpression[+In, ?Yield, ?Await] )
        import . source ( AssignmentExpression[+In, ?Yield, ?Await] )

---*/

$DONOTEVALUATE();

(async () => await import.UNKNOWN('./empty_FIXTURE.js'))

/* The params region intentionally empty */
