package com.chopchop3d.fspsender.dfs

import kotlin.math.max
import kotlin.math.roundToLong

data class FSPDryRunStats(
    var simulationThroughput : Double = 15.0, // MB/s
    var simulationEvaluation: Double = 0.0, // seconds
) {

    fun formattedDuration(): String =
        Formatter.formatDuration(simulationEvaluation)

    fun formattedThroughput(): String =
        Formatter.formatThroughput(simulationThroughput)

    companion object Formatter {

        /**
         * Smart formatted duration:
         *  5s
         *  01:15
         *  01:01:05
         *  1 01:01:01
         */
        fun formatDuration(secondsInput: Double): String {

            val totalSeconds = max(0.0, secondsInput).roundToLong()

            val days = totalSeconds / 86_400
            val hours = (totalSeconds % 86_400) / 3_600
            val minutes = (totalSeconds % 3_600) / 60
            val seconds = totalSeconds % 60

            return when {
                days > 0 -> String.format("%d %02d:%02d:%02d", days, hours, minutes, seconds)
                hours > 0 -> String.format("%02d:%02d:%02d", hours, minutes, seconds)
                minutes > 0 -> String.format("%02d:%02d", minutes, seconds)
                else -> String.format("%02ds", seconds)
            }
        }

        /**
         * Input is MB/s.
         * Displays KB/s if < 1 MB/s.
         */
        fun formatThroughput(mbPerSec: Double): String {
            return if (mbPerSec < 1.0) {
                val kb = mbPerSec * 1024.0
                String.format("%.1f KB/s", kb)
            } else {
                String.format("%.1f MB/s", mbPerSec)
            }
        }

        /**
         * Format file size in KB, MB, GB, TB (1024 base).
         */
        fun formatSize(bytes: Long): String {

            val kb = 1024.0
            val mb = kb * 1024
            val gb = mb * 1024
            val tb = gb * 1024

            val b = bytes.toDouble()

            return when {
                b >= tb -> String.format("%.2f TB", b / tb)
                b >= gb -> String.format("%.2f GB", b / gb)
                b >= mb -> String.format("%.2f MB", b / mb)
                else -> String.format("%.2f KB", b / kb)
            }
        }
    }
}
