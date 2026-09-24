// dual-model-bench: reCamera (SG2002) 双模型并发能力基准(客户 KPI 对照版)
//
// 场景与指标(对照 Veo 需求):
//   S1 单独运行语义分割:  端到端 ≤160ms(最低)/ ≤120ms(目标)
//   S2 YOLO+分割同时运行: 分割端到端 ≤200ms(最低)/ ≤160ms(目标)
//   S3 持续运行稳定性:    延迟无明显漂移、无过热降频
//
// 端到端(E2E)口径 = 预处理 + 推理 + 后处理 + 结果输出
//   前三段取自 ma::Model::getPerf()(ma_model_base.cpp 中三段计时),
//   "结果输出"为取回并遍历推理结果(结果序列化由上层业务决定,
//   此处以遍历结果代表输出开销)。
//
// 用法:
//   dual-model-bench [det.cvimodel] [seg.cvimodel] [iters] [soak_minutes]
//   - det/seg 默认:
//       det = /usr/share/supervisor/models/yolo11n_detection_cv181x_int8.cvimodel
//       seg = /usr/share/supervisor/models/yolo11n_segment_cv181x_int8.cvimodel
//   - iters 默认 100(S1/S2 每模型推理轮数,含 10 轮预热)
//   - soak_minutes 默认 0(>0 时执行 S3 持续运行测试)
//
// 运行前提(设备上):
//   sudo /etc/init.d/S91sscma-node stop      # 释放 TPU/ION,避免干扰
//   LD_LIBRARY_PATH=/lib:/usr/lib:/usr/lib64:/mnt/system/lib:/mnt/system/usr/lib:/mnt/system/usr/lib/3rd \
//       ./dual-model-bench [参数]
//
// 输出:各场景分段耗时统计(avg/P95/max)、KPI 逐条 PASS/FAIL 对照表。
// 详见同目录 README.md。

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <sscma.h>

namespace {

constexpr int kWarmupRounds = 10;

double now_ms() {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

struct Sample {
    double pre, inf, post, out;  // 分段(ms)
    double e2e() const { return pre + inf + post + out; }
};

struct Stats {
    std::vector<Sample> samples;
    double wall_ms = 0;

    void add(const Sample& s) { samples.push_back(s); }

    int n() const { return (int)samples.size(); }
    double avg_of(double Sample::*f) const {
        if (samples.empty()) return 0;
        double s = 0;
        for (auto& x : samples) s += x.*f;
        return s / samples.size();
    }
    // E2E 专用统计(成员函数不能取成员指针,单独实现)
    std::vector<double> e2e_sorted() const {
        std::vector<double> v;
        v.reserve(samples.size());
        for (auto& x : samples) v.push_back(x.e2e());
        std::sort(v.begin(), v.end());
        return v;
    }
    double e2e_avg() const {
        if (samples.empty()) return 0;
        double s = 0;
        for (auto& x : samples) s += x.e2e();
        return s / samples.size();
    }
    double e2e_p50() const {
        auto v = e2e_sorted();
        return v.empty() ? 0 : v[(size_t)((v.size() - 1) * 0.50)];
    }
    double e2e_p95() const {
        auto v = e2e_sorted();
        return v.empty() ? 0 : v[(size_t)((v.size() - 1) * 0.95)];
    }
    double e2e_max() const {
        double m = 0;
        for (auto& x : samples) m = std::max(m, x.e2e());
        return m;
    }
    double fps() const { return wall_ms > 0 ? n() * 1000.0 / wall_ms : 0; }

    void print(const char* tag) const {
        printf("  [%s] n=%d  pre=%.2f  infer=%.2f  post=%.2f  out=%.2f (ms,avg)\n",
               tag, n(), avg_of(&Sample::pre), avg_of(&Sample::inf),
               avg_of(&Sample::post), avg_of(&Sample::out));
        printf("        E2E: avg=%.2f  P50=%.2f  P95=%.2f  max=%.2f ms | %.2f fps | wall=%.0fms\n",
               e2e_avg(), e2e_p50(), e2e_p95(), e2e_max(), fps(), wall_ms);
    }
};

struct BenchModel {
    std::string name;
    std::string uri;
    ma::engine::EngineCVI* engine = nullptr;
    ma::Model* model = nullptr;
    ma_img_t input{};
    std::vector<uint8_t> buf;
    Stats solo;         // S1 / S2 各自统计
    Stats concurrent;
    int last_found = 0;
};

void makeTestImage(BenchModel& bm) {
    auto* in = reinterpret_cast<const ma_img_t*>(bm.model->getInput());
    int w = in->width, h = in->height;
    bm.buf.resize((size_t)w * h * 3);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t r = (uint8_t)(x * 255 / w), g = (uint8_t)(y * 255 / h), b = (uint8_t)((x + y) * 255 / (w + h));
            if (x > w / 5 && x < w * 2 / 5 && y > h / 5 && y < h * 3 / 5) { r = 220; g = 60; b = 50; }
            size_t i = ((size_t)y * w + x) * 3;
            bm.buf[i] = r; bm.buf[i + 1] = g; bm.buf[i + 2] = b;
        }
    }
    bm.input.data = bm.buf.data();
    bm.input.size = bm.buf.size();
    bm.input.width = w;
    bm.input.height = h;
    bm.input.format = MA_PIXEL_FORMAT_RGB888;
    bm.input.rotate = MA_PIXEL_ROTATE_0;
}

// 单帧完整流程:预处理→推理→后处理(框架内计时)+ 结果输出(取回遍历,自计时)
bool runOne(BenchModel& bm, Sample& s) {
    ma_err_t err = MA_FAILED;
    if (bm.model->getOutputType() == MA_OUTPUT_TYPE_BBOX) {
        err = static_cast<ma::model::Detector*>(bm.model)->run(&bm.input);
    } else if (bm.model->getOutputType() == MA_OUTPUT_TYPE_SEGMENT) {
        err = static_cast<ma::model::Segmentor*>(bm.model)->run(&bm.input);
    } else {
        return false;
    }
    if (err != MA_OK) return false;
    ma_perf_t p = bm.model->getPerf();

    // "结果输出":取回并遍历结果(代表输出开销;上层序列化另计)
    double t0 = now_ms();
    int found = 0;
    if (bm.model->getOutputType() == MA_OUTPUT_TYPE_BBOX) {
        auto& r = static_cast<ma::model::Detector*>(bm.model)->getResults();
        for (auto it = r.begin(); it != r.end(); ++it) found += (int)(it->x >= 0);
    } else if (bm.model->getOutputType() == MA_OUTPUT_TYPE_SEGMENT) {
        auto& r = static_cast<ma::model::Segmentor*>(bm.model)->getResults();
        for (auto it = r.begin(); it != r.end(); ++it) found += (int)(it->box.x >= 0);
    }
    s = {p.preprocess, p.inference, p.postprocess, now_ms() - t0};
    bm.last_found = found;
    return true;
}

void runRounds(BenchModel& bm, int iters, Stats& st, int warmup = 0) {
    Sample s;
    for (int i = 0; i < warmup; ++i) runOne(bm, s);  // 预热不计
    double t0 = now_ms();
    for (int i = 0; i < iters; ++i) {
        if (!runOne(bm, s)) { printf("[!] %s run 失败\n", bm.name.c_str()); break; }
        st.add(s);
    }
    st.wall_ms = now_ms() - t0;
}

void readTemp(char* out, size_t n) {
    out[0] = 0;
    FILE* f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!f) return;
    int t = -1;
    if (fscanf(f, "%d", &t) == 1 && t > 0) snprintf(out, n, "%.1fC", t / 1000.0);
    fclose(f);
}

void printHeader(const char* title) {
    printf("\n================ %s ================\n", title);
}

}  // namespace

int main(int argc, char** argv) {
    const char* det_uri = argc > 1 ? argv[1] : "/usr/share/supervisor/models/yolo11n_detection_cv181x_int8.cvimodel";
    const char* seg_uri = argc > 2 ? argv[2] : "/usr/share/supervisor/models/yolo11n_segment_cv181x_int8.cvimodel";
    int iters = argc > 3 ? atoi(argv[3]) : 100;
    int soak_min = argc > 4 ? atoi(argv[4]) : 0;

    printf("=== reCamera 双模型基准(YOLO 检测 + 语义分割)===\n");
    printf("YOLO(det): %s\n", det_uri);
    printf("SEG      : %s\n", seg_uri);
    printf("iters=%d(+%d 预热)  soak=%dmin\n", iters, kWarmupRounds, soak_min);

    BenchModel bm[2] = {{"YOLO-det", det_uri, nullptr, nullptr}, {"SEG", seg_uri, nullptr, nullptr}};
    for (auto& b : bm) {
        b.engine = new ma::engine::EngineCVI();
        if (b.engine->init() != MA_OK || b.engine->load(b.uri) != MA_OK) {
            printf("[FAIL] %s 加载失败: %s\n", b.name.c_str(), b.uri);
            return 1;
        }
        b.model = ma::ModelFactory::create(b.engine);
        if (!b.model || b.model->getInputType() != MA_INPUT_TYPE_IMAGE) {
            printf("[FAIL] %s 创建/输入类型失败\n", b.name.c_str());
            return 1;
        }
        makeTestImage(b);
        auto* in = reinterpret_cast<const ma_img_t*>(b.model->getInput());
        printf("  %-8s type=%u input=%dx%d outType=%u\n",
               b.name.c_str(), b.model->getType(), in->width, in->height, b.model->getOutputType());
    }

    // ---------- S1:单独运行(逐个独占)----------
    printHeader("S1 单独运行(各模型独占 TPU)");
    for (auto& b : bm) {
        runRounds(b, iters, b.solo, kWarmupRounds);
        b.solo.print(b.name.c_str());
        printf("        输出对象数(末帧): %d\n", b.last_found);
    }
    Stats& seg_solo = bm[1].solo;

    // ---------- S2:YOLO 与分割同时运行 ----------
    printHeader("S2 YOLO 与分割同时运行(双线程持续并发)");
    std::atomic<bool> stop{false};
    Stats seg_conc, det_conc;
    double det_wall = 0;

    std::thread det_thread([&] {
        Sample s;
        double t0 = now_ms();
        while (!stop.load()) {
            if (!runOne(bm[0], s)) break;
            det_conc.add(s);
        }
        det_wall = now_ms() - t0;
    });
    std::thread seg_thread([&] {
        Sample s;
        double t0 = now_ms();
        while (!stop.load()) {
            if (!runOne(bm[1], s)) break;
            seg_conc.add(s);
        }
        seg_conc.wall_ms = now_ms() - t0;
    });
    // 分割跑满 iters 轮后停止双方
    while (seg_conc.n() < iters) std::this_thread::sleep_for(std::chrono::milliseconds(20));
    stop.store(true);
    seg_thread.join();
    det_thread.join();
    det_conc.wall_ms = det_wall;

    det_conc.print("YOLO 并发");
    seg_conc.print("SEG  并发");
    printf("        并发期间: SEG 完成 %d 帧(%.2f fps), YOLO 完成 %d 帧(%.2f fps)\n",
           seg_conc.n(), seg_conc.fps(), det_conc.n(), det_conc.fps());

    // ---------- S3:持续运行稳定性(可选)----------
    Stats seg_soak;
    std::vector<std::pair<int, double>> windows;  // (起始秒, 窗口 E2E avg)
    if (soak_min > 0) {
        printHeader("S3 持续运行稳定性(YOLO+分割并发)");
        char tmp[32];
        readTemp(tmp, sizeof(tmp));
        printf("  起始温度: %s, 时长 %d 分钟...\n", tmp, soak_min);

        Stats d2;
        std::atomic<bool> stop2{false};
        std::thread dt([&] {
            Sample s;
            while (!stop2.load()) {
                if (!runOne(bm[0], s)) break;
                d2.add(s);
            }
        });
        int total_sec = soak_min * 60;
        int win_sec = 30;
        int start_n = 0;
        for (int t = 0; t < total_sec; t += win_sec) {
            int n0 = seg_soak.n();
            double t0 = now_ms();
            // 分割以 ~10fps 节流跑(贴近业务帧率,避免空转刷上限)
            while (now_ms() - t0 < win_sec * 1000.0) {
                Sample s;
                if (!runOne(bm[1], s)) break;
                seg_soak.add(s);
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
            }
            double avg = 0;
            int cnt = seg_soak.n() - n0;
            for (int i = 0; i < cnt; ++i) avg += seg_soak.samples[n0 + i].e2e();
            if (cnt) avg /= cnt;
            windows.push_back({t, avg});
            readTemp(tmp, sizeof(tmp));
            printf("  [%3d~%3ds] SEG E2E avg=%6.2fms  n=%d  temp=%s\n", t, t + win_sec, avg, cnt, tmp);
        }
        stop2.store(true);
        dt.join();

        double first = windows.empty() ? 0 : windows.front().second;
        double lastw = windows.empty() ? 0 : windows.back().second;
        printf("  首窗 avg=%.2fms -> 末窗 avg=%.2fms (漂移 %+.1f%%)\n",
               first, lastw, first > 0 ? (lastw - first) / first * 100 : 0);
    }

    // ---------- KPI 对照报告 ----------
    printHeader("KPI 对照结论(Veo 需求)");
    auto verdict = [](double v, double limit) { return v <= limit ? "PASS" : "FAIL"; };
    printf("  %-46s %8s %8s  %s\n", "指标", "实测", "要求", "判定");
    printf("  %-46s %7.1fms %7.1fms  %s\n", "S1 分割单独端到端(avg)", seg_solo.e2e_avg(), 160.0, verdict(seg_solo.e2e_avg(), 160.0));
    printf("  %-46s %7.1fms %7.1fms  %s\n", "S1 分割单独端到端(目标)", seg_solo.e2e_avg(), 120.0, verdict(seg_solo.e2e_avg(), 120.0));
    printf("  %-46s %7.1fms %7.1fms  %s\n", "S2 同时运行分割端到端(avg)", seg_conc.e2e_avg(), 200.0, verdict(seg_conc.e2e_avg(), 200.0));
    printf("  %-46s %7.1fms %7.1fms  %s\n", "S2 同时运行分割端到端(目标)", seg_conc.e2e_avg(), 160.0, verdict(seg_conc.e2e_avg(), 160.0));
    printf("  %-46s %7.1fms        -\n", "S2 同时运行分割端到端(P95)", seg_conc.e2e_p95());
    printf("  %-46s %7.1fms        -\n", "S2 同时运行 YOLO 端到端(avg)", det_conc.e2e_avg());
    if (soak_min > 0 && windows.size() >= 2) {
        double first = windows.front().second, lastw = windows.back().second;
        double drift = first > 0 ? (lastw - first) / first * 100 : 0;
        printf("  %-46s %+7.1f%%      -  %s\n", "S3 持续运行延迟漂移(首->末窗)", drift,
               (drift < 15.0) ? "PASS(<15%)" : "CHECK");
    }
    printf("\n注: E2E = 预处理+推理+后处理+结果输出;测试输入为合成图(无相机/编码干扰)。\n");
    printf("    Veo 参考基准: 其当前平台语义分割全流程约 160ms/frame。\n");

    for (auto& b : bm) {
        delete b.model;
        delete b.engine;
    }
    return 0;
}
