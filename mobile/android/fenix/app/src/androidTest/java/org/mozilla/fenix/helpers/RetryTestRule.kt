/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.helpers

import android.util.Log
import androidx.test.espresso.IdlingResourceTimeoutException
import androidx.test.espresso.NoMatchingViewException
import androidx.test.uiautomator.UiObjectNotFoundException
import junit.framework.AssertionFailedError
import leakcanary.NoLeakAssertionFailedError
import org.junit.rules.TestRule
import org.junit.runner.Description
import org.junit.runners.model.Statement
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.helpers.Constants.TAG
import org.mozilla.fenix.helpers.IdlingResourceHelper.unregisterAllIdlingResources
import org.mozilla.fenix.helpers.TestHelper.appContext
import org.mozilla.fenix.helpers.TestHelper.exitMenu

/**
 *  Rule to retry flaky tests for a given number of times, catching some of the more common exceptions.
 *  The Rule doesn't clear the app state in between retries, so we are doing some cleanup here.
 *  The @Before and @After methods are not called between retries.
 *
 */
class RetryTestRule(private val retryCount: Int = 5) : TestRule {
    @Suppress("TooGenericExceptionCaught", "ComplexMethod")
    override fun apply(base: Statement, description: Description): Statement {
        return statement {
            for (i in 1..retryCount) {
                try {
                    Log.i(TAG, "RetryTestRule: Started try #$i.")
                    base.evaluate()
                    break
                } catch (t: NoLeakAssertionFailedError) {
                    Log.i(TAG, "RetryTestRule: NoLeakAssertionFailedError caught, not retrying")
                    unregisterAllIdlingResources()
                    appContext.components.useCases.tabsUseCases.removeAllTabs()
                    exitMenu()
                    throw t
                } catch (t: AssertionError) {
                    Log.i(TAG, "RetryTestRule: AssertionError caught, retrying the UI test")
                    unregisterAllIdlingResources()
                    appContext.components.useCases.tabsUseCases.removeAllTabs()
                    exitMenu()
                    if (i == retryCount) {
                        Log.i(TAG, "RetryTestRule: Max numbers of retries reached.")
                        throw t
                    }
                } catch (t: AssertionFailedError) {
                    Log.i(TAG, "RetryTestRule: AssertionFailedError caught, retrying the UI test")
                    unregisterAllIdlingResources()
                    exitMenu()
                    if (i == retryCount) {
                        Log.i(TAG, "RetryTestRule: Max numbers of retries reached.")
                        throw t
                    }
                } catch (t: UiObjectNotFoundException) {
                    Log.i(TAG, "RetryTestRule: UiObjectNotFoundException caught, retrying the UI test")
                    unregisterAllIdlingResources()
                    exitMenu()
                    if (i == retryCount) {
                        Log.i(TAG, "RetryTestRule: Max numbers of retries reached.")
                        throw t
                    }
                } catch (t: NoMatchingViewException) {
                    Log.i(TAG, "RetryTestRule: NoMatchingViewException caught, retrying the UI test")
                    unregisterAllIdlingResources()
                    exitMenu()
                    if (i == retryCount) {
                        Log.i(TAG, "RetryTestRule: Max numbers of retries reached.")
                        throw t
                    }
                } catch (t: IdlingResourceTimeoutException) {
                    Log.i(TAG, "RetryTestRule: IdlingResourceTimeoutException caught, retrying the UI test")
                    unregisterAllIdlingResources()
                    exitMenu()
                    if (i == retryCount) {
                        Log.i(TAG, "RetryTestRule: Max numbers of retries reached.")
                        throw t
                    }
                } catch (t: RuntimeException) {
                    Log.i(TAG, "RetryTestRule: RuntimeException caught, retrying the UI test")
                    unregisterAllIdlingResources()
                    exitMenu()
                    if (i == retryCount) {
                        Log.i(TAG, "RetryTestRule: Max numbers of retries reached.")
                        throw t
                    }
                } catch (t: NullPointerException) {
                    Log.i(TAG, "RetryTestRule: NullPointerException caught, retrying the UI test")
                    unregisterAllIdlingResources()
                    exitMenu()
                    if (i == retryCount) {
                        Log.i(TAG, "RetryTestRule: Max numbers of retries reached.")
                        throw t
                    }
                }
            }
        }
    }

    private inline fun statement(crossinline eval: () -> Unit): Statement {
        return object : Statement() {
            override fun evaluate() = eval()
        }
    }
}
