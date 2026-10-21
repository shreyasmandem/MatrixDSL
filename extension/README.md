# MatrixDSL Lab: Chrome extension and dashboard

**Owner:** Shreyas Mandem (24BCE0381)

A visual front end for the real compiler. Write or pick a MatrixDSL program;
the backend compiles it four times, runs every version on the MDT simulator,
verifies every output, and the dashboard shows:

* **Traditional vs MatrixDSL.** Three cards compared side by side:
  * a scalar core's analytic lower bound (one multiply-accumulate per cycle);
  * the traditional compile, executed (matrix unit, but no fusion and every
    tile fetched from DRAM);
  * MatrixDSL, executed (fusion + scratchpad tiling).

  Below the cards: an animated race, the simulator's transfer/compute
  timeline on a shared cycle axis, all four configurations, and the program's
  output as a heat map.
* **Ablation suite.** The eight benchmark workloads × four configurations,
  run and verified live, with geo-mean speed-ups.
* **Inside the compiler.** Tensor IR before and after the passes, and the
  traditional vs optimised MDT assembly.

## Run it

```bash
cmake -S . -B build && cmake --build build -j
build/bin/mtxserve                 # serves 127.0.0.1:8765 (local only)
```

Then do either of the following:

* **Browser tab.** Open <http://127.0.0.1:8765/>.
* **Chrome extension.**
  1. Open `chrome://extensions` and turn on **Developer mode**.
  2. Click **Load unpacked** and select this `extension/` folder.
  3. Click the MatrixDSL Lab toolbar icon, then **Open dashboard**.

Both show the same page (`extension/dashboard/`). The extension only has
permission to talk to `127.0.0.1:8765`, and nothing leaves the machine.

## API (tools/mtxserve)

| Endpoint | Returns |
|---|---|
| `GET /api/health` | backend version |
| `GET /api/programs` | bundled workloads, examples and test programs |
| `POST /api/run` (body = program text) | for each configuration: cycles, DRAM/scratchpad bytes, instructions, Tensor IR, assembly, timeline, verification; plus the outputs |
| `GET /api/suite` | the full ablation study |

"Traditional" means the *same* compiler front end and the *same* MDT hardware,
compiled without tensor-level optimisation. The comparison therefore isolates
what the compiler's fusion and tiling contribute. Cycle counts come from MDT's
analytical model (not cycle-accurate hardware), and the scalar-core figure is
an analytic lower bound, labelled as such.
