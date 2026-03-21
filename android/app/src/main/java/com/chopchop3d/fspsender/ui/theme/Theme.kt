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

// Brighter cyan tweak
val CyberPrimaryBright = Color(0xFF3CE0E0)      // more neon cyan
val CyberOnPrimaryBright = Color(0xFF002F2F)    // darker contrasting text
// -----------------------------
// Cyber Primary Button (bright cyan, slightly smaller, bold font)
// -----------------------------
@Composable
fun CyberButton(
    onClick: () -> Unit,
    text: String,
    modifier: Modifier = Modifier
) {
    Button(
        onClick = onClick,
        modifier = modifier
            .height(48.dp), // slightly smaller than 56.dp
        shape = RoundedCornerShape(10.dp), // slightly less rounded
        colors = ButtonDefaults.buttonColors(
            containerColor = CyberPrimaryBright,
            contentColor = CyberOnPrimaryBright
        ),
        elevation = ButtonDefaults.buttonElevation(defaultElevation = 4.dp) // a bit subtler
    ) {
        Text(
            text = text,
            style = MaterialTheme.typography.labelLarge.copy(
                fontSize = 16.sp,             // slightly smaller font
                fontWeight = FontWeight.Bold, // still bold, but not ExtraBold
                letterSpacing = 0.6.sp        // subtle cyber feel
            ),
            modifier = Modifier.padding(horizontal = 6.dp)
        )
    }
}