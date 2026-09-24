# dual-model-bench — reCamera 双模型(YOLO 检测 + 语义分割)基准测试

对 SG2002 (reCamera) 进行 **YOLO 目标检测 + 语义分割双模型** 的性能基准,
按 Veo 侧需求逐条给出 PASS/FAIL 判定。不依赖相机与 OpenCV,输入为合成图,
结果不受采集/编码链路干扰,可复现。

## 性能需求(Veo)

| # | 指标 | 最低要求 | 目标 |
|---|------|---------|------|
| S1 | 语义分割单独运行,端到端延迟 | ≤ 160 ms | ≤ 120 ms |
| S2 | YOLO 与语义分割同时运行,分割端到端延迟 | ≤ 200 ms | ≤ 160 ms |
| S3 | 长时间持续运行,无明显散热/降频性能下降 | 无明显下降 | — |

> Veo 侧参考:其当前平台语义分割完整流程(预处理+推理+后处理+结果输出)约 **160 ms/frame**。

## 端到端(E2E)计时口径

```
E2E = 预处理 + 推理 + 后处理 + 结果输出
```

- 前三段由 sscma-micro 框架在 `Model::run()` 内计时(`ma_model_base.cpp`,
  单位 ms),经 `Model::getPerf()` 读取;
- "结果输出"为本程序取回并遍历推理结果的耗时(代表输出开销;
  上层业务若需 JPEG 编码/网络发送,另行叠加);
- 统计输出 avg / P50 / P95 / max 与 fps。

## 编译

前置:`SG200X_SDK_PATH` 指向 reCamera SDK 的 `output/sg2002_recamera_emmc`,
工具链 `host-tools/gcc/riscv64-linux-musl-x86_64/bin` 在 PATH 中。

```bash
cd solutions/dual_model_bench
cmake -B build -DCMAKE_BUILD_TYPE=Release . && cmake --build build -j
# 产物: build/dual-model-bench (riscv64)
```

## 部署与运行(设备上)

```bash
# 1) 上传
scp -P 8822 build/dual-model-bench recamera@<设备IP>:/tmp/

# 2) 停掉 sscma-node 服务(独占 TPU/ION,避免干扰计时)
ssh recamera@<设备IP>
sudo /etc/init.d/S91sscma-node stop

# 3) 运行(必须带 LD_LIBRARY_PATH)
sudo LD_LIBRARY_PATH=/lib:/usr/lib:/usr/lib64:/mnt/system/lib:/mnt/system/usr/lib:/mnt/system/usr/lib/3rd \
    /tmp/dual-model-bench [det.cvimodel] [seg.cvimodel] [iters] [soak_minutes]

# 4) 测完恢复服务
sudo /etc/init.d/S91sscma-node start
```

## 参数

| 位置 | 参数 | 默认 | 说明 |
|-----|------|------|------|
| 1 | det.cvimodel | `/usr/share/supervisor/models/yolo11n_detection_cv181x_int8.cvimodel` | YOLO 检测模型 |
| 2 | seg.cvimodel | `/usr/share/supervisor/models/yolo11n_segment_cv181x_int8.cvimodel` | 语义分割模型(可换 Veo 模型) |
| 3 | iters | 100 | S1/S2 每模型统计轮数(另有 10 轮预热不计时) |
| 4 | soak_minutes | 0 | >0 时执行 S3 持续运行测试(分钟) |

示例 — 用 Veo 模型 + 5 分钟长稳:

```bash
sudo LD_LIBRARY_PATH=... /tmp/dual-model-bench \
    /usr/share/supervisor/models/yolo11n_detection_cv181x_int8.cvimodel \
    /tmp/models_releases_v007_vision2.cvimodel 100 5
```

## 输出解读

程序依次输出:

1. **模型信息** — SSCMA 识别的模型类型、输入尺寸、输出类型
   (用于确认模型被正确加载;例如 Veo 模型识别为 type=17
   `MA_MODEL_TYPE_BISENETV2`,输入 640×320,输出 SEGMENT);
2. **S1 单独运行** — 每模型独占 TPU 的分段统计与 E2E 分布;
3. **S2 双线程同时运行** — YOLO 与分割持续并发,分割跑满 iters 轮
   (此为"同时运行"业务口径:两模型都持续在跑,统计分割侧延迟);
4. **S3 持续稳定性**(可选)— 分割按 ~10fps 业务节流 + YOLO 满速并发,
   每 30s 输出一窗 E2E 均值与 SoC 温度,末尾给出首末窗漂移百分比;
5. **KPI 对照结论** — 逐条 PASS/FAIL。

## 已知结论(2026-09 实测,yolo11n int8 + Veo vision2 640×320)

- Veo 语义分割模型单独运行 E2E ≈ **58 ms**(推理 43ms),
  同时与 YOLO 运行时 E2E ≈ **95~115 ms**,均满足最低与目标要求;
- SG2002 TPU 为硬件串行调度:双线程并发不提升总吞吐
  (并发合计吞吐低于串行交替约 40%),但**延迟达标**;
  若追求最大吞吐,建议单线程交替调度两模型;
- S1/S2 数据详见 stdout;长稳(S3)以首末窗漂移 <15% 为 PASS。

## 注意事项

- 运行前务必停 sscma-node,否则 TPU/ION 竞争导致计时失真甚至 OOM;
- 模型输入为按其声明尺寸生成的合成图(渐变+色块):计时有效,
  但检测/分割"结果内容"不代表真实画面精度 — 精度评估请接真实图像;
- 温度读取自 `/sys/class/thermal/thermal_zone0/temp`;
- 若需真实相机流输入的版本,可参考 `solutions/sscma-node` 的
  camera/model 节点实现改造。
