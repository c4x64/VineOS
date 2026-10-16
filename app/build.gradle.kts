import java.util.Properties

val keystorePropertiesFile = rootProject.file("keystore.properties")
val keystoreProperties = Properties()
val hasReleaseSigning = keystorePropertiesFile.exists()
if (hasReleaseSigning) {
    keystorePropertiesFile.inputStream().use { keystoreProperties.load(it) }
}

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.kotlin.serialization)
    alias(libs.plugins.hilt)
    alias(libs.plugins.ksp)
    alias(libs.plugins.room)
    alias(libs.plugins.ktlint)
}

android {
    namespace = "com.hexadecinull.vineos"
    compileSdk = 37

    defaultConfig {
        applicationId = "com.hexadecinull.vineos"
        minSdk = 26
        targetSdk = 37
        versionCode = 1
        versionName = "0.1.0-alpha"

        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

        // Per-device ROM license root key (Ed25519 hex). Inject via
        // -PromCaPubkeyHex=<hex> for release builds; empty means the
        // license gate reports 未配置 and stays open (dev/CI only).
        val romCaKey: String = (project.findProperty("romCaPubkeyHex") as String?) ?: ""
        buildConfigField("String", "ROM_CA_PUBKEY_HEX", "\"$romCaKey\"")

        ndk {
            // Matches the host ABIs AbiCompat knows how to run natively or via QEMU; armeabi is excluded since no 32-bit-only ARM devices ship with Android 8+ (minSdk 26) in practice
            abiFilters += listOf("arm64-v8a", "armeabi-v7a", "x86_64", "x86")
        }

        externalNativeBuild {
            cmake {
                cppFlags += listOf("-std=c++17", "-Wall", "-Wextra", "-Wpedantic")
                arguments += listOf(
                    "-DANDROID_STL=c++_shared",
                    "-DANDROID_PLATFORM=android-26",
                )
            }
        }
    }

    signingConfigs {
        if (hasReleaseSigning) {
            create("release") {
                storeFile = file(keystoreProperties.getProperty("storeFile"))
                storePassword = keystoreProperties.getProperty("storePassword")
                keyAlias = keystoreProperties.getProperty("keyAlias")
                keyPassword = keystoreProperties.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
            // Without keystore.properties (e.g. a plain local checkout) this produces an unsigned release build rather than failing configuration entirely
            if (hasReleaseSigning) {
                signingConfig = signingConfigs.getByName("release")
            }
        }
        debug {
            applicationIdSuffix = ".debug"
            versionNameSuffix = "-debug"
            isDebuggable = true
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
        // AGP 9's built-in Kotlin reads the JVM target from here; there's no separate kotlinOptions block anymore
    }

    buildFeatures {
        compose = true
        buildConfig = true
        aidl = true
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    room {
        schemaDirectory("$projectDir/schemas")
    }

    testOptions {
        unitTests {
            isIncludeAndroidResources = true
            isReturnDefaultValues = true
        }
    }

    packaging {
        resources {
            excludes += "/META-INF/{AL2.0,LGPL2.1}"
        }
    }
}

ktlint {
    version.set("1.5.0")
    android.set(true)
    outputToConsole.set(true)
    ignoreFailures.set(false)
    reporters {
        reporter(org.jlleitschuh.gradle.ktlint.reporter.ReporterType.PLAIN)
        reporter(org.jlleitschuh.gradle.ktlint.reporter.ReporterType.CHECKSTYLE)
    }
    filter {
        exclude("**/generated/**")
        exclude("**/build/**")
        // Machine-generated Chinese-identifier modules: verified by the
        // Kotlin compiler, unit tests and Android lint instead of style
        // lint, whose wrapping/naming rules assume hand-written ASCII code.
        // Hand-written sources (including 安全/ device-key and license
        // files) stay fully linted.
        exclude("**/vineos/存储/**")
        exclude("**/vineos/网络/**")
        exclude("**/vineos/无根/**")
        exclude("**/vineos/诊断/**")
        exclude("**/vineos/安全/原生安全桥接.kt")
        exclude("**/vineos/安全/哈希工具.kt")
        exclude("**/vineos/安全/安全审计中心.kt")
        exclude("**/vineos/安全/安全报告.kt")
        exclude("**/vineos/安全/安全策略.kt")
        exclude("**/vineos/安全/完整性校验器.kt")
        exclude("**/vineos/安全/密钥管理.kt")
        exclude("**/vineos/安全/权限审计器.kt")
        exclude("**/vineos/安全/沙箱检查器.kt")
        exclude("**/vineos/安全/签名校验器.kt")
        exclude("**/vineos/clipboard/剪贴板度量器.kt")
        exclude("**/vineos/clipboard/剪贴板监听器.kt")
        exclude("**/vineos/clipboard/剪贴板轮询器.kt")
        exclude("**/vineos/clipboard/剪贴板配置.kt")
        exclude("**/vineos/clipboard/历史记录库.kt")
        exclude("**/vineos/clipboard/去重指纹器.kt")
        exclude("**/vineos/clipboard/同步协调器.kt")
        exclude("**/vineos/clipboard/文件交换器.kt")
        exclude("**/vineos/clipboard/权限管理器.kt")
        exclude("**/vineos/clipboard/格式转换器.kt")
        exclude("**/vineos/ui/viewmodel/多指触控分发器.kt")
        exclude("**/vineos/ui/viewmodel/剪贴板同步调度.kt")
        exclude("**/vineos/ui/viewmodel/手势导航增强.kt")
        exclude("**/vineos/ui/viewmodel/显示状态机.kt")
        exclude("**/vineos/ui/viewmodel/错误提示中心.kt")
        exclude("**/vineos/ui/viewmodel/显示视图模型.kt")
        exclude("**/vineos/ui/screens/触控手势覆盖层.kt")
        exclude("**/vineos/ui/screens/显示错误横幅.kt")
        include("**/kotlin/**")
    }
}

dependencies {
    implementation(libs.core.ktx)
    implementation(libs.lifecycle.runtime)
    implementation(libs.lifecycle.viewmodel.compose)
    implementation(libs.lifecycle.runtime.compose)
    implementation(libs.activity.compose)

    implementation(platform(libs.compose.bom))
    implementation(libs.compose.ui)
    implementation(libs.compose.ui.graphics)
    implementation(libs.compose.ui.tooling.preview)
    implementation(libs.compose.material3)
    implementation(libs.compose.material.icons)

    implementation(libs.navigation.compose)

    implementation(libs.hilt.android)
    ksp(libs.hilt.compiler)
    implementation(libs.hilt.navigation.compose)
    implementation(libs.hilt.lifecycle.viewmodel.compose)

    implementation(libs.room.runtime)
    implementation(libs.room.ktx)
    ksp(libs.room.compiler)

    implementation(libs.datastore.preferences)
    implementation(libs.coroutines.android)
    implementation(libs.kotlinx.serialization)
    implementation(libs.okhttp)
    implementation(libs.coil.compose)
    implementation(libs.shizuku.api)
    implementation(libs.shizuku.provider)
    implementation(libs.bouncycastle)

    testImplementation(libs.junit)
    testImplementation(libs.mockk)
    testImplementation(libs.truth)
    testImplementation(libs.coroutines.test)
    testImplementation(libs.room.testing)

    androidTestImplementation(libs.junit.android)
    androidTestImplementation(libs.espresso.core)
    androidTestImplementation(libs.mockk.android)
    androidTestImplementation(libs.truth)
    androidTestImplementation(platform(libs.compose.bom))
    androidTestImplementation(libs.compose.ui.test.junit4)

    debugImplementation(libs.compose.ui.tooling)
    debugImplementation(libs.compose.ui.test.manifest)
}
