package com.chopchop3d.fspsender.ui.theme

import androidx.compose.foundation.text.selection.LocalTextSelectionColors
import androidx.compose.foundation.text.selection.TextSelectionColors
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

// -----------------------------
// Color Scheme
// -----------------------------
private val CyberDarkColors = darkColorScheme(
    primary = CyberPrimary,
    onPrimary = CyberOnPrimary,

    secondary = CyberSecondary,
    onSecondary = CyberOnSecondary,

    tertiary = CyberTertiary,
    onTertiary = CyberOnTertiary,

    background = CyberBackground,
    onBackground = CyberTextPrimary,

    surface = CyberBackground,
    onSurface = CyberTextPrimary,

    surfaceVariant = CyberSurface,
    outline = CyberOutline,

    error = CyberError,
    onError = CyberBackground
)

// -----------------------------
// Theme
// -----------------------------
@Composable
fun FSPSenderTheme(
    content: @Composable () -> Unit
) {
    MaterialTheme(
        colorScheme = CyberDarkColors,
        typography = Typography,
        content = {
            CompositionLocalProvider(
                LocalContentColor provides CyberTextPrimary,
                LocalTextSelectionColors provides TextSelectionColors(
                    handleColor = CyberPrimary,
                    backgroundColor = CyberPrimary.copy(alpha = 0.3f)
                )
            ) {
                content()
            }
        }
    )
}

// -----------------------------
// Cyber Card (no borders)
// -----------------------------
@Composable
fun CyberCard(content: @Composable () -> Unit) {
    Card(
        colors = CardDefaults.cardColors(
            containerColor = CyberSurfaceContainer
        ),
        elevation = CardDefaults.cardElevation(0.dp)
    ) {
        content()
    }
}

// -----------------------------
// Cyber Primary Button
// -----------------------------
@Composable
fun CyberButton(
    onClick: () -> Unit,
    content: @Composable () -> Unit
) {
    Button(
        onClick = onClick,
        colors = ButtonDefaults.buttonColors(
            containerColor = CyberPrimary,
            contentColor = CyberOnPrimary
        )
    ) {
        content()
    }
}