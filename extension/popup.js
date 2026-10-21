// MatrixDSL Lab - extension popup. Owner: Shreyas Mandem (24BCE0381)
"use strict";

const status = document.getElementById("status");

async function check() {
  status.className = "status";
  status.querySelector("span").textContent = "Checking the compiler backend…";
  try {
    const r = await fetch("http://127.0.0.1:8765/api/health");
    const h = await r.json();
    status.className = "status on";
    status.querySelector("span").textContent = "Backend connected · MatrixDSL " + h.version;
    document.getElementById("help").hidden = true;
  } catch (e) {
    status.className = "status off";
    status.querySelector("span").textContent = "Backend not running";
    document.getElementById("help").hidden = false;
  }
}

document.getElementById("open").addEventListener("click", () => {
  chrome.tabs.create({ url: chrome.runtime.getURL("dashboard/index.html") });
  window.close();
});
document.getElementById("retry").addEventListener("click", check);
check();
