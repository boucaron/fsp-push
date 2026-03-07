package com.chopchop3d.fspsender


data class SshConfig(
    val host: String,
    val username: String,
    val password: String,
    val port: Int = 22
)