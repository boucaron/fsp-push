package com.chopchop3d.fspsender.ui.theme

import androidx.compose.foundation.text.selection.TextSelectionColors
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.material3.OutlinedTextFieldDefaults


// -----------------------------
// Zenburn Dark Palette
// -----------------------------
private val ZenburnDarkColors = darkColorScheme(
    primary = Color(0xFF4C7073),      // zenburn-blue-4 (darker main accent)
    secondary = Color(0xFF5C888B),    // zenburn-blue-3 (soft accent)
    tertiary = Color(0xFF94BFF3),     // zenburn-blue+1 (lighter accent for highlights)
    background = Color(0xFF3F3F3F),   // zenburn-bg
    surface = Color(0xFF2B2B2B),      // zenburn-bg-1
    onPrimary = Color(0xFFDCDCCC),    // zenburn-fg
    onSecondary = Color(0xFFDCDCCC),  // zenburn-fg
    onTertiary = Color(0xFFDCDCCC),   // zenburn-fg
    onBackground = Color(0xFFDCDCCC), // zenburn-fg
    onSurface = Color(0xFFDCDCCC),    // zenburn-fg
    error = Color(0xFFCC9393),        // zenburn-red
    onError = Color(0xFF2B2B2B)       // dark background for contrast
)
// -----------------------------
// Zenburn Dark Theme
// -----------------------------
@Composable
fun FSPSenderTheme(
    content: @Composable () -> Unit
) {
    MaterialTheme(
        colorScheme = ZenburnDarkColors,
        typography = Typography,
        content = {
            CompositionLocalProvider(
                LocalContentColor provides ZenburnDarkColors.onBackground
            ) {
                content()
            }
        }
    )
}

// -----------------------------
// Zenburn Card
// -----------------------------
@Composable
fun ZenburnCard(content: @Composable () -> Unit) {
    Card(
        colors = CardDefaults.cardColors(containerColor = ZenburnDarkColors.surface),
        elevation = CardDefaults.cardElevation(defaultElevation = 0.dp)
    ) {
        content()
    }
}

// -----------------------------
// Zenburn Button
// -----------------------------
@Composable
fun ZenburnButton(onClick: () -> Unit, content: @Composable () -> Unit) {
    Button(
        onClick = onClick,
        colors = ButtonDefaults.buttonColors(
            containerColor = ZenburnDarkColors.primary,
            contentColor = ZenburnDarkColors.onPrimary
        )
    ) {
        content()
    }
}

// -----------------------------
// Zenburn OutlinedTextField
// -----------------------------
@Composable
fun ZenburnTextField(
    value: String,
    onValueChange: (String) -> Unit,
    placeholder: String = ""
) {
    OutlinedTextField(
        value = value,
        onValueChange = onValueChange,
        placeholder = { Text(placeholder) },
        colors = OutlinedTextFieldDefaults.colors(
            focusedTextColor = ZenburnDarkColors.onSurface,
            unfocusedTextColor = ZenburnDarkColors.onSurface,
            disabledTextColor = ZenburnDarkColors.onSurface.copy(alpha = 0.3f),
            errorTextColor = ZenburnDarkColors.error,

            cursorColor = ZenburnDarkColors.primary,
            errorCursorColor = ZenburnDarkColors.error,
            selectionColors = TextSelectionColors(
                handleColor = ZenburnDarkColors.primary,
                backgroundColor = ZenburnDarkColors.primary.copy(alpha = 0.4f)
            ),

            focusedContainerColor = ZenburnDarkColors.surface,
            unfocusedContainerColor = ZenburnDarkColors.surface,
            disabledContainerColor = ZenburnDarkColors.surface,
            errorContainerColor = ZenburnDarkColors.surface,

            focusedBorderColor = ZenburnDarkColors.primary,
            unfocusedBorderColor = ZenburnDarkColors.onSurface.copy(alpha = 0.3f),
            disabledBorderColor = ZenburnDarkColors.onSurface.copy(alpha = 0.1f),
            errorBorderColor = ZenburnDarkColors.error,

            focusedLeadingIconColor = ZenburnDarkColors.onSurface,
            unfocusedLeadingIconColor = ZenburnDarkColors.onSurface.copy(alpha = 0.7f),
            disabledLeadingIconColor = ZenburnDarkColors.onSurface.copy(alpha = 0.3f),
            errorLeadingIconColor = ZenburnDarkColors.error,

            focusedTrailingIconColor = ZenburnDarkColors.onSurface,
            unfocusedTrailingIconColor = ZenburnDarkColors.onSurface.copy(alpha = 0.7f),
            disabledTrailingIconColor = ZenburnDarkColors.onSurface.copy(alpha = 0.3f),
            errorTrailingIconColor = ZenburnDarkColors.error,

            focusedPlaceholderColor = ZenburnDarkColors.onSurface.copy(alpha = 0.5f),
            unfocusedPlaceholderColor = ZenburnDarkColors.onSurface.copy(alpha = 0.5f),
            disabledPlaceholderColor = ZenburnDarkColors.onSurface.copy(alpha = 0.3f),
            errorPlaceholderColor = ZenburnDarkColors.error,

            focusedLabelColor = ZenburnDarkColors.onSurface,
            unfocusedLabelColor = ZenburnDarkColors.onSurface.copy(alpha = 0.7f),
            disabledLabelColor = ZenburnDarkColors.onSurface.copy(alpha = 0.3f),
            errorLabelColor = ZenburnDarkColors.error,

            focusedSupportingTextColor = ZenburnDarkColors.onSurface,
            unfocusedSupportingTextColor = ZenburnDarkColors.onSurface.copy(alpha = 0.7f),
            disabledSupportingTextColor = ZenburnDarkColors.onSurface.copy(alpha = 0.3f),
            errorSupportingTextColor = ZenburnDarkColors.error,

            focusedPrefixColor = ZenburnDarkColors.onSurface,
            unfocusedPrefixColor = ZenburnDarkColors.onSurface.copy(alpha = 0.7f),
            disabledPrefixColor = ZenburnDarkColors.onSurface.copy(alpha = 0.3f),
            errorPrefixColor = ZenburnDarkColors.error,

            focusedSuffixColor = ZenburnDarkColors.onSurface,
            unfocusedSuffixColor = ZenburnDarkColors.onSurface.copy(alpha = 0.7f),
            disabledSuffixColor = ZenburnDarkColors.onSurface.copy(alpha = 0.3f),
            errorSuffixColor = ZenburnDarkColors.error
        )
    )
}