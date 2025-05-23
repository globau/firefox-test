/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.app.links

import androidx.fragment.app.DialogFragment

/**
 * This is a general representation of a dialog meant to be used in collaboration with [AppLinksInterceptor]
 * to show a dialog before an external link is opened.
 * If [SimpleRedirectDialogFragment] is not flexible enough for your use case you should inherit for this class.
 * Be mindful to call [onConfirmRedirect] when you want to open the linked app.
 */
abstract class RedirectDialogFragment : DialogFragment() {

    /**
     * A callback to trigger a redirect action. Call it when you are ready to open the linked app. For instance,
     * a valid use case can be in confirmation dialog, after the positive button is clicked,
     * this callback must be called.
     *
     * checkboxChecked - Boolean parameter indicating if the checkbox is ticked or not when clicking
     * on the positive button
     */
    var onConfirmRedirect: (checkboxChecked: Boolean) -> Unit = {}

    /**
     * A callback to trigger when user dismisses the dialog.
     * For instance, a valid use case can be in confirmation dialog, after the negative button is clicked,
     * this callback must be called.
     */
    var onCancelRedirect: () -> Unit? = {}

    companion object {
        const val FRAGMENT_TAG = "SHOULD_OPEN_APP_LINK_PROMPT_DIALOG"
    }
}
