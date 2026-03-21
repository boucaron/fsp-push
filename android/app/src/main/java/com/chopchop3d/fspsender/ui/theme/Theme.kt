package com.chopchop3d.fspsender.ui.theme

import androidx.compose.foundation.layout.height
import androidx.compose.foundation.text.selection.LocalTextSelectionColors
import androidx.compose.foundation.text.selection.TextSelectionColors
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp

import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Text
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.sp

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
// Cyber Primary Button (fatter, larger font)
// -----------------------------
@Composable
fun CyberButton(
    onClick: () -> Unit,
    text: String,                // pass text directly
    modifier: Modifier = Modifier // allow external Modifier
) {
    Button(
        onClick = onClick,
        modifier = modifier
            .height(56.dp), // taller button
        shape = RoundedCornerShape(12.dp),
        colors = ButtonDefaults.buttonColors(
            containerColor = CyberPrimary,
            contentColor = CyberOnPrimary
        ),
        elevation = ButtonDefaults.buttonElevation(defaultElevation = 6.dp)
    ) {
        Text(
            text = text,
            style = MaterialTheme.typography.labelLarge.copy(
                fontSize = 16.sp,                // larger font
                fontWeight = FontWeight.ExtraBold, // thicker font
                letterSpacing = 0.6.sp            // optional cyber feel
            ),
            modifier = Modifier.padding(horizontal = 6.dp)
        )
    }
}