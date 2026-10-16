package com.hexadecinull.vineos.诊断

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.launch

// 版权所有：藤蔓操作系统诊断子系统
// 文件功能：诊断总览界面——Compose 展示采集结果
class 诊断视图模型(val 仓储: 诊断仓储 = 诊断仓储()) : ViewModel() {
    var 快照文本 by mutableStateOf("尚未采集")
        private set
    var 性能文本 by mutableStateOf("-")
        private set
    var 内存文本 by mutableStateOf("-")
        private set
    var 线程文本 by mutableStateOf("-")
        private set
    var 错误列表 by mutableStateOf(listOf<错误分组>())
        private set
    var 日志列表 by mutableStateOf(listOf<日志条目>())
        private set
    fun 刷新全部() {
        viewModelScope.launch {
            val 快照 = 仓储.生成诊断快照()
            快照文本 = 快照.转为多行()
            性能文本 = 快照.性能摘要
            内存文本 = 快照.内存摘要
            线程文本 = 快照.线程摘要
            错误列表 = 仓储.查询错误热门(20)
            日志列表 = 仓储.查询日志(日志查询条件(上限 = 100))
        }
    }
    fun 采集内存快照() {
        viewModelScope.launch {
            val 快照 = 仓储.采集内存快照()
            内存文本 = 快照.转为日志行()
        }
    }
    fun 上报测试错误() {
        仓储.上报错误("手动测试", "用户点击测试按钮", "栈：诊断总览界面.上报测试错误")
        刷新全部()
    }
}

@Composable fun 诊断总览界面(视图模型: 诊断视图模型 = 诊断视图模型()) {
    LaunchedEffect(Unit) { 视图模型.刷新全部() }
    LazyColumn(modifier = Modifier.fillMaxSize().padding(16.dp)) {
        item {
            Text("诊断总览", style = MaterialTheme.typography.headlineSmall)
            Spacer(Modifier.height(8.dp))
        }
        item { 卡片展示("性能", 视图模型.性能文本) }
        item { 卡片展示("内存", 视图模型.内存文本) }
        item { 卡片展示("线程", 视图模型.线程文本) }
        item { Button(onClick = { 视图模型.刷新全部() }) { Text("刷新全部") } }
        item { Button(onClick = { 视图模型.采集内存快照() }) { Text("采集内存快照") } }
        items(视图模型.错误列表, key = { it.指纹 }) { 分组 -> 错误分组卡片(分组) }
        items(视图模型.日志列表.take(30)) { 日志 -> Text("${日志.标签}:${日志.内容.take(80)}") }
    }
}

@Composable fun 卡片展示(标题: String, 内容: String) {
    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Column(Modifier.padding(12.dp)) {
            Text(标题, style = MaterialTheme.typography.titleMedium)
            Text(内容.take(500))
        }
    }
}

@Composable fun 错误分组卡片(分组: 错误分组) {
    Card(modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp)) {
        Column(Modifier.padding(12.dp)) {
            Text("${分组.类别} ×${分组.次数}")
            Text(分组.代表摘要.take(200))
        }
    }
}
fun 通用格式化展示(原文: String): String {
    if (原文.isEmpty()) return "暂无"
    val 清洗 = 原文.replace("\r", "").trim()
    return if (清洗.length > 300)清洗.substring(0, 300) + "…" else 清洗
}

@Composable fun 通用占位卡片(标题: String) {
    Card(Modifier.fillMaxWidth().padding(4.dp)) { Text(标题.take(200)) }
}
fun 格式化展示文本1(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片1(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本2(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片2(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本3(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片3(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本4(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片4(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本5(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片5(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本6(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片6(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本7(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片7(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本8(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片8(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本9(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片9(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本10(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片10(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本11(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片11(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本12(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片12(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本13(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片13(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本14(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片14(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本15(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片15(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本16(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片16(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本17(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片17(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本18(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片18(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本19(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片19(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本20(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片20(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本21(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片21(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本22(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片22(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本23(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片23(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本24(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片24(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本25(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片25(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本26(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片26(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本27(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片27(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本28(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片28(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本29(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片29(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本30(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片30(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本31(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片31(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本32(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片32(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本33(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片33(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本34(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片34(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本35(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片35(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本36(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片36(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本37(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片37(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本38(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片38(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本39(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片39(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本40(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片40(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本41(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片41(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本42(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片42(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本43(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片43(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本44(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片44(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本45(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片45(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本46(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片46(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本47(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片47(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本48(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片48(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本49(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片49(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本50(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片50(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本51(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片51(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本52(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片52(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本53(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片53(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本54(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片54(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本55(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片55(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本56(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片56(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本57(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片57(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本58(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片58(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本59(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片59(标题: String) = 通用占位卡片(标题)

fun 格式化展示文本60(原文: String): String = 通用格式化展示(原文)

@Composable fun 占位卡片60(标题: String) = 通用占位卡片(标题)
