const disbl = "disabled";
const classHide = "visually-hidden";
const apiLink = "/api?action=";

function el(tag, opts = {}) {
	const node = document.createElement(tag);
	if (opts["class"]) node.className = opts["class"];
	if (opts.id) node.id = opts.id;
	if (opts.text) node.textContent = opts.text;
	if (opts.html) node.innerHTML = opts.html;
	if (opts.style) node.setAttribute("style", opts.style);
	if (opts.type) node.type = opts.type;
	if (opts.rows) node.rows = opts.rows;
	if (opts.disabled !== undefined) node.disabled = true;
	if (opts.role) node.setAttribute("role", opts.role);
	if (opts.append) { const a = Array.isArray(opts.append) ? opts.append : [opts.append]; a.forEach(n => node.appendChild(n)); }
	if (opts.click) node.addEventListener("click", opts.click);
	return node;
}

function createSpinner() {
	const s = el("span", { role: "status", "class": "spinner spinner-border text-primary spinner-border-sm" });
	s.style.marginLeft = "10px";
	return s;
}

const zbFwInfoUrl = "https://raw.githubusercontent.com/AVATTO-smart/AVATTO_GW_firmware/main/avatto_fws.json";

const GITHUB_REPO_OWNER = "AVATTO-smart";
const GITHUB_REPO_NAME = "avatto-gw90-firmware";

const pages = {
	API_PAGE_ROOT: { num: 0, str: "/", title: "Status" },
	API_PAGE_GENERAL: { num: 1, str: "/general", title: "General" },
	API_PAGE_ETHERNET: { num: 2, str: "/ethernet", title: "Ethernet" },
	API_PAGE_WIFI: { num: 3, str: "/wifi", title: "WiFi" },
	API_PAGE_SERIAL: { num: 4, str: "/zha-z2m", title: "ZHA/Z2M" },
	API_PAGE_SECURITY: { num: 5, str: "/security", title: "Security" },
	API_PAGE_SYSTOOLS: { num: 6, str: "/sys-tools", title: "Tools" },
	API_PAGE_ABOUT: { num: 7, str: "/about", title: "About" },
	API_PAGE_MQTT: { num: 8, str: "/mqtt", title: "MQTT Configuration" },
	API_PAGE_WG: { num: 9, str: "/wg", title: "WireGuard" }
};

const api = {
	actions: {
		API_GET_PAGE: 0,
		API_GET_PARAM: 1,
		API_STARTWIFISCAN: 2,
		API_WIFISCANSTATUS: 3,
		API_WIFICONNECTSTAT: 5,
		API_CMD: 6,
		API_GET_LOG: 7,
		API_FLASH_ZB: 8
	},
	pages: pages
};

function showModal(show) {
	const modal = document.getElementById("modal");
	if (modal) {
		if (show) {
			modal.classList.add("show");
			modal.style.display = "block";
		} else {
			modal.classList.remove("show");
			modal.style.display = "none";
		}
	}
}

function toggleCollapse(id, show) {
	const el = document.getElementById(id);
	if (el) {
		if (show) el.classList.add("show");
		else el.classList.remove("show");
	}
}

function getEl(id) { return document.getElementById(id); }
function getQ(sel) { return document.querySelector(sel); }
function getQA(sel) { return document.querySelectorAll(sel); }

function initBsHandlers() {
	document.addEventListener("click", function (e) {
		const tabBtn = e.target.closest("[data-bs-toggle='tab']");
		if (tabBtn) { e.preventDefault(); switchTab(tabBtn); return; }
		const dismissModal = e.target.closest("[data-bs-dismiss='modal']");
		if (dismissModal) { showModal(false); return; }
		const modal = getEl("modal");
		if (modal && e.target === modal && modal.getAttribute("data-bs-backdrop") !== "static") { showModal(false); return; }
	});
	document.addEventListener("keydown", function (e) {
		if (e.key === "Escape") {
			const modal = getEl("modal");
			if (modal && modal.classList.contains("show") && modal.getAttribute("data-bs-keyboard") !== "false") showModal(false);
		}
	});
}

document.addEventListener("DOMContentLoaded", function () {
	const activeLink = document.querySelector("a[href='" + document.location.pathname + "']");
	if (activeLink && activeLink.parentElement) activeLink.parentElement.classList.add("nav-active");
	loadPage(document.location.pathname);

	if (isMobile()) {
		if (!(localStorage.getItem("shv_sdnv_frst_t") == 1)) {
			const sidenav = getEl("sidenav");
			if (sidenav) { sidenav.classList.add("sidenav-active"); localStorage.setItem("shv_sdnv_frst_t", 1); setTimeout(() => sidenav.classList.remove("sidenav-active"), 2000); }
		}
		setupSwipeHandler();
		const pc = getEl("pageContent");
		if (pc) pc.classList.remove("container");
	}

	getQA("a.nav-link").forEach(a => {
		a.addEventListener("click", function (e) {
			e.preventDefault();
			const url = this.getAttribute("href");
			if (url == "/logout") { window.location = "/logout"; return; }
			loadPage(url);
			getQA(".nav-active").forEach(n => n.classList.remove("nav-active"));
			if (this.parentElement) this.parentElement.classList.add("nav-active");
			if (isMobile()) sidenavAutoclose(true);
		});
	});

	initLanguage();
	initDarkModeToggle();
	initBsHandlers();
});

function copyCode() {
	let textArea = getEl("generatedFile");
	if (!navigator.clipboard) {
		textArea.focus();
		textArea.select();
		try {
			let successful = document.execCommand("copy");
			console.log("Fallback: Copying text command was " + (successful ? "successful" : "unsuccessful"));
		} catch (err) {
			console.error("Fallback: Oops, unable to copy", err);
		}
	} else {
		navigator.clipboard.writeText(textArea.value).then(function () {
			console.log("Async: Copying to clipboard was successful!");
		}, function (err) {
			console.error("Async: Could not copy text: ", err);
		});
	}
	const clipIco = getEl("clipIco");
	if (clipIco) clipIco.setAttribute("xlink:href", "icons.svg#clipboard2-check");
}

function generateConfig(params) {
	let result;
	const ip = window.location.host;
	const port = getEl("port").value;
	if (ip == "192.168.1.1") { const ap = getEl("apAlert"); if (ap) ap.classList.remove(classHide); }
	switch (params) {
		case "zha":
			result = "socket://" + ip + ":" + port;
			break;
		case "z2m":
			result = `# Serial settings
serial:
  # Location of AVATTO-GW90-Ti
  port: tcp://${ip}:${port}
  adapter: zstack
  baudrate: ${getEl("baud").value}
  # Disable Zigbee (Y or W) led?
  disable_led: false
# Set output power to max 20
advanced:
  transmit_power: 20`;
			break;
		case "usb":
			result = `# For homeassistant: Go to "Settings"→"System"→"Hardware"→Select the 3 dot menu in the upper right corner→"All Hardware"→Scroll to ttyUSB and find your adapter→Copy Device path like "/dev/ttyUSB0"
# List USB devices on Linux: ls  /dev/ttyUSB*
serial:
# Location of AVATTO-GW90-Ti
  port: INSERT_DEVICE_PATCH_HERE
  baudrate: ${getEl("baud").value}
# Disable green led?
  disable_led: false
# Set output power to max 20
advanced:
  transmit_power: 20`;
			break;
		default:
			break;
	}
	getEl("generatedFile").value = result;
}

function loadPage(url) {
	window.history.pushState("", document.title, url);
	console.log("[loadPage] url: " + url);
	window.scrollTo(0, 0);
	switch (url) {
		case api.pages.API_PAGE_ROOT.str:
			apiGetPage(api.pages.API_PAGE_ROOT);
			break;
		case api.pages.API_PAGE_GENERAL.str:
			apiGetPage(api.pages.API_PAGE_GENERAL, () => {
				if (!getEl("serialMode").checked) KeepWebDsbl(true);
			});
			break;
		case api.pages.API_PAGE_ETHERNET.str:
			apiGetPage(api.pages.API_PAGE_ETHERNET, () => {
				if (getEl("EthDhcpTog").checked) EthInputDsbl(true);
			});
			break;
		case api.pages.API_PAGE_MQTT.str:
			apiGetPage(api.pages.API_PAGE_MQTT, () => {
				if (getEl("MqttEnable").checked == false) MqttInputDsbl(true);
			});
			break;
		case api.pages.API_PAGE_WG.str:
			apiGetPage(api.pages.API_PAGE_WG, () => {
				if (getEl("WgEnable").checked == false) WgInputDsbl(true);
			});
			break;
		case api.pages.API_PAGE_WIFI.str:
			apiGetPage(api.pages.API_PAGE_WIFI, () => {
				if (getEl("dhcpWiFi").checked) WifiDhcpDsbl(true);
				else WifiDhcpDsbl(false);
				initWifiPage();
			});
			break;
		case api.pages.API_PAGE_SERIAL.str:
			apiGetPage(api.pages.API_PAGE_SERIAL, () => { generateConfig("z2m"); });
			break;
		case api.pages.API_PAGE_SECURITY.str:
			apiGetPage(api.pages.API_PAGE_SECURITY, () => {
				if (getEl("webAuth").checked) SeqInputDsbl(false);
				if (getEl("fwEnabled").checked) SeqInputDsblFw(false);
			});
			break;
		case api.pages.API_PAGE_SYSTOOLS.str:
			apiGetPage(api.pages.API_PAGE_SYSTOOLS, () => {
				const hz = getEl("humanizeSw");
				if (hz) {
					hz.checked = localStorage.getItem("humanize") !== "0";
					hz.addEventListener("change", () => {
						localStorage.setItem("humanize", hz.checked ? "1" : "0");
						repaintLogs();
					});
				}
				const seg = getEl("logFilterSeg");
				if (seg) {
					setLogFilter(logFilterMode, seg);
					seg.querySelectorAll("button").forEach(b => b.addEventListener("click", () => {
						setLogFilter(b.dataset.logfilter, seg);
						repaintLogs();
					}));
				}
				const clr = getEl("clearLogBtn");
				fetch(apiLink + api.actions.API_GET_PARAM + "&param=refreshLogs").then(r => r.text()).then(data => {
					logRefresh(parseInt(data) >= 1000 ? parseInt(data) : 1000);
				});
			});
			break;
		case api.pages.API_PAGE_ABOUT.str:
			apiGetPage(api.pages.API_PAGE_ABOUT, () => { checkEspFirmware(); checkZbFirmware(); });
			break;
		default:
			apiGetPage(api.pages.API_PAGE_ROOT);
			break;
	}
	if (isMobile()) sidenavAutoclose(true);
}

function espReboot() {
	fetch(apiLink + api.actions.API_CMD + "&cmd=3");
}

function apiGetPage(page, doneCall) {
	const animDuration = 200;
	const locCall = doneCall;
	showPreloader(true);
	const pc = getEl("pageContent");
	pc.style.transition = "opacity " + animDuration + "ms";
	pc.style.opacity = "0";
	setTimeout(() => {
		fetch(apiLink + api.actions.API_GET_PAGE + "&page=" + page.num).then(r => {
			if (!r.ok) throw new Error(r.status + " " + r.statusText);
			const authHeader = r.headers.get("Authentication");
			const respValues = r.headers.get("respValuesArr");
			const respTimeZones = r.headers.get("respTimeZones");
			return r.text().then(text => ({ text, authHeader, respValues, respTimeZones }));
		}).then(({ text, authHeader, respValues, respTimeZones }) => {
			pc.innerHTML = text;
			pc.querySelectorAll("script").forEach(s => {
				const ns = document.createElement("script");
				if (s.src) ns.src = s.src;
				else ns.textContent = s.textContent;
				s.parentNode.replaceChild(ns, s);
			});
			showPreloader(false);
			if (authHeader == "ok") getQA(".logoutLink").forEach(l => l.classList.remove(classHide));
			pc.style.opacity = "1";

			getQA("form.saveParams").forEach(form => {
				form.addEventListener("submit", function (e) {
					e.preventDefault();
					const btn = form.querySelector("button[type='submit']");
					getQA(":disabled").forEach(d => d.removeAttribute("disabled"));
					const sp = createSpinner();
					const sp2 = el("span", { "class": classHide });
					btn.appendChild(sp);
					btn.appendChild(sp2);
					btn.disabled = true;
					const formData = new FormData(form);
					formData.append("pageId", page.num);
					fetch(e.currentTarget.action, { method: "POST", body: formData }).then(() => {
						modalConstructor("saveOk");
					}).catch(() => {
						alert("Error saving settings. Check your network");
					}).finally(() => {
						sp.remove();
						sp2.remove();
						btn.disabled = false;
					});
				});
			});

			getQA("button").forEach(btn => {
				btn.addEventListener("click", function () {
					const btnStndrt = "btn-outline-primary";
					const btnGrn = "btn-outline-success";
					const cmd = this.getAttribute("data-cmd");
					if (cmd) {
						if (cmd === "3") { modalConstructor("confirmRestart"); return; }
						if (cmd === "10" || cmd === "11") { // results print into the event log
							const seg = getEl("logFilterSeg");
							if (seg && logFilterMode !== "esp") { setLogFilter("esp", seg); repaintLogs(); }
						}
						const sp = createSpinner();
						const sp2 = el("span", { "class": classHide });
						this.appendChild(sp);
						this.appendChild(sp2);
						this.disabled = true;
						fetch(apiLink + api.actions.API_CMD + "&cmd=" + cmd).then(() => {
							this.classList.remove(btnStndrt);
							this.classList.add(btnGrn);
							const self = this;
							setTimeout(() => {
								self.classList.remove(btnGrn);
								self.classList.add(btnStndrt);
							}, 1000);
						}).catch(() => {
							alert("Error. Check your network");
						}).finally(() => {
							const self = this;
							setTimeout(() => {
								sp.remove();
								sp2.remove();
								self.disabled = false;
							}, 700);
						});
					}
				});
			});

			getQA("[data-replace='pageName']").forEach(e => e.textContent = page.title);
			const titleEl = document.querySelector("title[data-replace='pageName']");
			if (titleEl) titleEl.textContent = page.title + " - AVATTO-GW90-Ti Zigbee Ethernet POE USB Adapter";

			if (respValues === null) return;
			console.log("[apiGetPage] starting parse values");
			const values = JSON.parse(respValues);
			let selectedTimeZone = null;
			for (const property in values) {
				if (property === "timeZoneName") {
					selectedTimeZone = values[property];
					console.debug(selectedTimeZone);
					console.debug("timeZoneName");
					continue;
				}
				getQA("[data-replace='" + property + "']").forEach(elem => {
					const elemType = elem.nodeName.toLowerCase();
					let valueToSet = values[property];
					const isIpValue = /^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$/.test(valueToSet);
					const isMaskInPropertyName = property.toLowerCase().includes("mask");
					if (isIpValue && !isMaskInPropertyName) {
						valueToSet = '<a href="http://' + valueToSet + '">' + valueToSet + '</a>';
					}
					switch (elemType) {
						case "input":
						case "select":
						case "textarea":
							const type = (elem.type || "").toLowerCase();
							if (elemType == "input" && (type == "checkbox" || type == "radio")) {
								elem.checked = values[property];
							} else {
								elem.value = values[property];
							}
							break;
						case "option":
							elem.selected = true;
							break;
						default:
							if (isIpValue && !isMaskInPropertyName) {
								elem.innerHTML = valueToSet;
							} else {
								const pill = statusPill(valueToSet);
								if (pill) elem.innerHTML = pill;
								else elem.textContent = valueToSet;
							}
							break;
					}
				});
			}

			if (respTimeZones !== null) {
				const zones = JSON.parse(respTimeZones);
				const dropdown = getEl("timeZoneId");
				if (dropdown) {
					dropdown.innerHTML = "";
					if (Array.isArray(zones)) {
						zones.forEach(item => {
							let option = new Option(item, item);
							if (item === selectedTimeZone) option.selected = true;
							dropdown.appendChild(option);
						});
					} else {
						console.error("zones is not an array");
					}
				}
			}

			if (values.operational_mode || values.operationalMode) {
				const modeVal = values.operationalMode || values.operational_mode;
				const mode = modeVal.toLowerCase();
				const dd = getEl("deviceDiagram");
				if (dd) {
					dd.classList.remove("mode-lan", "mode-wifi", "mode-usb");
					if (mode.includes("wifi")) dd.classList.add("mode-wifi");
					else if (mode.includes("usb")) dd.classList.add("mode-usb");
					else dd.classList.add("mode-lan");
				}
			}

			// WiFi configured but idle (Ethernet active): tint the fallback
			// icon/line blue so "asleep" reads at a glance.
			const diagram = getEl("deviceDiagram");
			if (diagram) {
				if (["set up", "asleep"].includes(String(values.wifiConnected || "").toLowerCase())) diagram.classList.add("wifi-ready");
				else diagram.classList.remove("wifi-ready");
			}

			if (typeof locCall == "function") locCall();
		}).catch(err => {
			alert("Page load error: " + err.message);
		});
	}, animDuration);
}

function showPreloader(state) {
	const pre = getEl("uzPreloader");
	if (pre) {
		if (state) pre.classList.remove(classHide);
		else pre.classList.add(classHide);
	}
}

function applyTheme(dark) {
	if (dark) document.documentElement.setAttribute("data-theme", "dark");
	else document.documentElement.removeAttribute("data-theme");
	localStorage.setItem("theme", dark ? "dark" : "light");
}

function toggleDarkModeMobile() {
	applyTheme(document.documentElement.getAttribute("data-theme") !== "dark");
}

function initDarkModeToggle() {
	const saved = localStorage.getItem("theme");
	const prefersDark = window.matchMedia("(prefers-color-scheme: dark)").matches;
	const isDark = saved !== null ? saved === "dark" : prefersDark;
	if (isDark) document.documentElement.setAttribute("data-theme", "dark");
	else document.documentElement.removeAttribute("data-theme");
	const btn = document.getElementById("darkModeToggleSide");
	if (btn) btn.addEventListener("click", toggleDarkModeMobile);
}

function closeModal() {
	showModal(false);
}
function rebootWait() {
	setTimeout(() => { modalConstructor("rebootWait"); console.log("[rebootWait] start"); }, 1000);
}

function espFlashGitWait() {
	ESPfwStartEvents();
	setTimeout(() => {
		fetch(apiLink + api.actions.API_CMD + "&cmd=9");
		console.log("[flash] start");
		const prg = getEl("prg");
		if (prg) prg.innerHTML = "Github download starting...";
	}, 500);
}

function zbFlashGitWait() {
	if (!latestZbFirmware) return;
	console.log("Starting Zigbee update from Git... Please wait patiently!");
	startZbFlashFromGit();
}

let zbEventSource = null;
let zbEventConnectTimeout = null;
let originalRefreshInterval = null;

function startZbFlashFromGit() {
	console.log("Starting Zigbee flash from Git...");
	if (window.logRefreshInterval) {
		originalRefreshInterval = window.logRefreshInterval;
		clearInterval(window.logRefreshInterval);
	}
	closeModal();
	setTimeout(() => {
		const fwTab = getEl("fw-tab");
		if (fwTab) switchTab(fwTab);
		const prgZb = getEl("prg_zb");
		const barZb = getEl("bar_zb");
		const updBtnZb = getEl("updButton_zb");
		if (prgZb) prgZb.innerHTML = "Preparing device...";
		if (barZb) barZb.style.width = "0%";
		if (updBtnZb) updBtnZb.disabled = true;
		ZBfwStartEvents();
		setTimeout(() => {
			let firmwareUrl = latestZbFirmware.link;
			console.log("Firmware URL:", firmwareUrl);
			if (!firmwareUrl) {
				console.error("Firmware URL is empty!");
				if (prgZb) prgZb.innerHTML = '<span style="color:red">Invalid firmware link</span>';
				if (updBtnZb) updBtnZb.disabled = false;
				return;
			}
			startFlashProcess(firmwareUrl);
		}, 2000);
	}, 300);
}

function switchTab(tabEl) {
	const tabLinks = getQA("[data-bs-toggle='tab']");
	tabLinks.forEach(t => t.classList.remove("active"));
	tabEl.classList.add("active");
	const target = tabEl.getAttribute("data-bs-target");
	if (target) {
		getQA(".tab-pane").forEach(p => {
			p.classList.remove("active", "show");
		});
		const pane = getQ(target);
		if (pane) pane.classList.add("active", "show");
	}
}

function startFlashProcess(fwUrl) {
	console.log("Starting flash process with URL:", fwUrl);
	const prgZb = getEl("prg_zb");
	const barZb = getEl("bar_zb");
	if (prgZb) prgZb.innerHTML = "Downloading firmware...";
	if (barZb) barZb.style.width = "5%";
	fetch(apiLink + api.actions.API_FLASH_ZB + "&fwurl=" + encodeURIComponent(fwUrl)).then(r => r.text()).then(data => {
		console.log("API Response:", data);
	}).catch(err => {
		console.error("Download failed:", err);
		if (prgZb) prgZb.innerHTML = '<span style="color:red">Download failed: ' + err + "</span>";
		if (barZb) barZb.style.width = "0%";
		const updBtnZb = getEl("updButton_zb");
		if (updBtnZb) updBtnZb.disabled = false;
	});
}

let espEventSource = null;
function ESPfwStartEvents() {
	if (espEventSource) espEventSource.close();
	espEventSource = new EventSource("/events");
	console.log("Events try");
	espEventSource.addEventListener("open", () => console.log("Events Connected"));
	espEventSource.addEventListener("error", e => { if (e.target.readyState != EventSource.OPEN) console.log("Events Err"); });
	espEventSource.addEventListener("ESP_FW_prgs", e => {
		const val = e.data + "%";
		console.log("[ESP_FW_prgs] " + val);
		const prg = getEl("prg");
		const bar = getEl("bar");
		if (prg) prg.innerHTML = "progress: " + Math.round(e.data) + "%";
		if (bar) bar.style.width = Math.round(e.data) + "%";
		if (Math.round(e.data) > 99.5) {
			setTimeout(() => {
				if (prg) prg.innerHTML = "Update completed!<br>Rebooting!";
				rebootWait();
			}, 250);
		}
	});
}

function ZBfwStartEvents() {
	if (zbEventSource) { zbEventSource.close(); zbEventSource = null; }
	if (zbEventConnectTimeout) { clearTimeout(zbEventConnectTimeout); zbEventConnectTimeout = null; }
	zbEventSource = new EventSource("/events");
	console.log("Events try");
	zbEventConnectTimeout = setTimeout(() => {
		console.log("EventSource connection timeout");
		const prgZb = getEl("prg_zb");
		if (prgZb) prgZb.innerHTML = '<span style="color:orange">EventSource timeout</span>';
		if (zbEventSource) { zbEventSource.close(); zbEventSource = null; }
	}, 60000);
	zbEventSource.addEventListener("open", () => {
		console.log("Events Connected");
		if (zbEventConnectTimeout) { clearTimeout(zbEventConnectTimeout); zbEventConnectTimeout = null; }
	});
	zbEventSource.addEventListener("error", e => {
		console.log("Events Err");
		if (zbEventConnectTimeout) { clearTimeout(zbEventConnectTimeout); zbEventConnectTimeout = null; }
		if (e.target.readyState != EventSource.OPEN) {
			console.log("EventSource connection failed, using polling mode...");
			const prgZb = getEl("prg_zb");
			if (prgZb) prgZb.innerHTML = '<span style="color:orange">Connection failed, using polling mode...</span>';
			if (zbEventSource) { zbEventSource.close(); zbEventSource = null; }
		}
	});
	zbEventSource.addEventListener("ZB_FW_downloading", e => {
		const pct = parseInt(e.data);
		console.log("Downloaded: " + pct + "%");
		const prgZb = getEl("prg_zb");
		const barZb = getEl("bar_zb");
		if (prgZb) prgZb.innerHTML = "Downloading... " + pct + "%";
		if (barZb) barZb.style.width = pct + "%";
		if (pct >= 100) {
			if (prgZb) prgZb.innerHTML = "Download complete, starting flash...";
			if (barZb) barZb.style.width = "100%";
		}
	});
	zbEventSource.addEventListener("ZB_FW_flashing", e => {
		const pct = Math.min(parseInt(e.data), 100);
		console.log("Flashing progress: " + pct + "%");
		const prgZb = getEl("prg_zb");
		const barZb = getEl("bar_zb");
		if (prgZb) prgZb.innerHTML = "Flashing... " + pct + "%";
		if (barZb) barZb.style.width = pct + "%";
		if (pct >= 100) {
			setTimeout(() => {
				if (prgZb) prgZb.innerHTML = '<span style="color:green">Flash complete!</span>';
				if (zbEventSource) { zbEventSource.close(); zbEventSource = null; }
				const updBtnZb = getEl("updButton_zb");
				if (updBtnZb) updBtnZb.disabled = false;
				if (originalRefreshInterval) window.logRefreshInterval = originalRefreshInterval;
			}, 250);
		}
	});
	zbEventSource.addEventListener("ZB_FW_info", e => {
		console.log("Info:", e.data);
		if (e.data.includes("Validating")) {
			const prgZb = getEl("prg_zb");
			const barZb = getEl("bar_zb");
			if (prgZb) prgZb.innerHTML = "Validating and flashing...";
			if (barZb) barZb.style.width = "0%";
		}
	});
	zbEventSource.addEventListener("ZB_FW_err", e => {
		console.error("Error:", e.data);
		const prgZb = getEl("prg_zb");
		if (prgZb) prgZb.innerHTML = '<span style="color:red">' + e.data + "</span>";
		const updBtnZb = getEl("updButton_zb");
		if (updBtnZb) updBtnZb.disabled = false;
		if (zbEventSource) { zbEventSource.close(); zbEventSource = null; }
	});
}

function modalConstructor(type, params) {
	console.log("[modalConstructor] start");
	const hText = getQ(".modal-title");
	const mBody = getQ(".modal-body");
	const mBtns = getQ(".modal-footer");
	if (hText) { hText.textContent = ""; hText.style.color = ""; }
	if (mBody) { mBody.textContent = ""; mBody.style.color = ""; }
	if (mBtns) mBtns.innerHTML = "";
	switch (type) {
		case "flashWarning":
			if (hText) { hText.textContent = "WARNING"; hText.style.color = "red"; }
			if (mBody) { mBody.textContent = "Flashing unofficial, incorrect or corrupted firmware can damage or brick your device!!!"; mBody.style.color = "red"; }
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-success", text: "Close", click: closeModal }));
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-danger", text: "I agree and I am taking the risk", click: () => { closeModal(); localStorage.setItem("flash_warning", 1); const ub = getEl("updButton"); if (ub) ub.removeAttribute("disabled"); } }));
			}
			break;
		case "confirmRestart":
			if (hText) hText.textContent = "Restart ESP32";
			if (mBody) mBody.textContent = "The device will restart and the connection will be interrupted for a few seconds. Continue?";
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-outline-secondary", text: "Cancel", click: closeModal }));
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-danger", text: "Restart", click: () => { closeModal(); espReboot(); rebootWait(); } }));
			}
			break;
		case "espFwDetails":
			if (hText) hText.textContent = "ESP32 firmware";
			if (mBody) { mBody.textContent = params.text; mBody.insertAdjacentHTML("beforeend", params.chglog); }
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-primary", text: "Close", click: closeModal }));
				if (params.hasUpdate) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: "Update now", click: () => { closeModal(); localStorage.setItem("update_notify", 0); espFlashGitWait(); } }));
			}
			break;
		case "espFlashGitWait":
			if (hText) hText.textContent = "ESP32 GIT UPDATE";
			if (mBody) mBody.textContent = "Downloading and flashing latest release from Github. This window will automatically close when the device reboots.";
			if (mBtns) mBtns.appendChild(el("div", { role: "status", "class": "spinner-border text-primary", append: el("span", { "class": classHide }) }));
			var waitTmr = setInterval(() => { fetch("/").then(() => { clearInterval(waitTmr); clearTimeout(timeoutTmr); closeModal(); console.log("[espFlashGitWait] hide modal"); window.location = "/"; }).catch(() => {}); }, 3000);
			var timeoutTmr = setTimeout(() => { clearInterval(waitTmr); if (mBtns) mBtns.innerHTML = ""; if (mBody) { mBody.textContent = "No response from the device, this may happen if the device changed IP address or if the USB mode was selected."; mBody.style.color = "red"; } if (mBtns) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: "Close", click: closeModal })); }, 90000);
			break;
		case "espGitUpdate":
			if (hText) { hText.textContent = "ESP32 update available"; hText.style.color = "#f7a623"; }
			if (mBody) mBody.textContent = params;
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-primary", text: "Later", click: () => { closeModal(); localStorage.setItem("update_notify", 0); } }));
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-danger", text: "Don't remind", click: () => { closeModal(); localStorage.setItem("update_notify", 1); } }));
			}
			break;
		case "zbFirmwareSelect":
			if (hText) hText.textContent = "Select Zigbee Firmware Type";
			if (mBody) mBody.innerHTML = "";
			if (mBtns) mBtns.innerHTML = "";
			if (mBody) mBody.appendChild(el("div", { "class": "container", style: "max-width: 400px", append: el("div", { "class": "row", id: "zbFirmwareSelectRow" }) }));
			const selRow = getEl("zbFirmwareSelectRow");
			if (selRow) {
				selRow.appendChild(el("button", { "class": "col-sm-12 mb-2 btn btn-primary", text: "Coordinator", click: () => showZigbeeVersionInfo("coordinator") }));
				selRow.appendChild(el("button", { "class": "col-sm-12 mb-2 btn btn-outline-primary", text: "Router", click: () => showZigbeeVersionInfo("router") }));
				selRow.appendChild(el("button", { type: "button", "class": "col-sm-12 btn btn-outline-secondary", text: "Cancel", click: closeModal }));
			}
			break;
		case "zbVersionInfo":
			if (hText) hText.textContent = "Latest Zigbee Firmware";
			if (mBody) { mBody.innerHTML = params.chglog; const alertDiv = el("div", { "class": "alert alert-info", html: params.text }); mBody.insertBefore(alertDiv, mBody.firstChild); }
		if (mBtns) {
			mBtns.appendChild(el("button", { type: "button", "class": "btn btn-primary", text: "Close", click: closeModal }));
			if (params.hasUpdate) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: "Update Now", click: () => { closeModal(); zbFlashGitWait(); } }));
		}
		break;
		case "rebootWait":
			if (hText) hText.textContent = "DEVICE RESTART";
			if (mBody) mBody.textContent = "Waiting for device... This window will automatically close when the device reboots.";
			if (mBtns) mBtns.appendChild(el("div", { role: "status", "class": "spinner-border text-primary", append: el("span", { "class": classHide }) }));
			var waitTmr = setInterval(() => { fetch("/").then(() => { clearInterval(waitTmr); clearTimeout(timeoutTmr); closeModal(); console.log("[rebootWait] hide modal"); window.location = "/"; }).catch(() => {}); }, 2000);
			var timeoutTmr = setTimeout(() => { clearInterval(waitTmr); if (mBtns) mBtns.innerHTML = ""; if (mBody) { mBody.textContent = "No response from the device, this may happen if the device changed IP address or if the USB mode was selected."; mBody.style.color = "red"; } if (mBtns) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: "Close", click: closeModal })); }, 20000);
			break;
		case "saveOk":
			if (window.location.pathname == "/wifi") {
				if (hText) hText.textContent = "Wi-Fi network connection";
				if (mBody) mBody.textContent = "Connecting to the network in progress... Wait for the result.";
				if (mBtns) mBtns.appendChild(el("div", { role: "status", "class": "spinner-border text-primary", append: el("span", { "class": classHide }) }));
				let counter = 0;
				var getWifiIp = setInterval(() => {
					if (counter <= 15) {
						fetch(apiLink + api.actions.API_WIFICONNECTSTAT).then(r => r.json()).then(data => {
							if (data.connected) {
								espReboot();
								clearInterval(getWifiIp);
								setTimeout(() => {
									if (mBody) mBody.innerHTML = '<span style="color: green">Connected!</span><br>New IP address is ' + data.ip + '<br>Device will now reboot for the new settings to take effect.';
									if (mBtns) mBtns.innerHTML = "";
									if (mBtns) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-success", text: "Go to " + data.ip, click: () => { window.location = "http://" + data.ip + "/"; } }));
								}, 5000);
							} else { counter++; }
						});
					} else {
						clearInterval(getWifiIp);
						if (mBody) { mBody.textContent = "Connection error, check SSID, PASSWORD and try again"; mBody.style.color = "red"; }
						if (mBtns) mBtns.innerHTML = "";
						if (mBtns) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-success", text: "Close", click: closeModal }));
					}
				}, 1000);
			} else {
				let body = "New parameters saved. ";
				if (hText) hText.textContent = "Settings saved";
				body += "Some settings require a reboot.";
				if (mBtns) {
					mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: "Restart manually later", click: closeModal }));
					mBtns.appendChild(el("button", { type: "button", "class": "btn btn-success", text: "Restart now", click: () => { closeModal(); espReboot(); rebootWait(); } }));
				}
				if (mBody) mBody.textContent = body;
			}
			break;
		default:
			break;
	}
	showModal(true);
}

function getWifiList() {
	const list = getEl("wifiList");
	// While scanning there is nothing to interact with - the join row only
	// appears once results are in, so a mid-scan join can't race the list.
	if (list) list.innerHTML = '<div class="wifi-item wifi-placeholder"><span class="wifi-spinner"></span><span class="wifi-name">Scanning…</span></div>';
	let polls = 0;
	const giveUp = () => {
		if (!list) return;
		list.innerHTML = '<div class="wifi-item wifi-placeholder" onclick="getWifiList()"><span class="wifi-name">Scan timed out. Tap to retry.</span></div>' + _wifiJoinRow;
	};
	fetch(apiLink + api.actions.API_STARTWIFISCAN).then(() => {
		const tmrUpdWifi = setInterval(() => {
			const expire = () => { clearInterval(tmrUpdWifi); giveUp(); }; // ~30s
			fetch(apiLink + api.actions.API_WIFISCANSTATUS).then(r => r.json()).then(data => {
				if (!data.scanDone) {
					if (++polls >= 15) expire();
					return;
				}
				clearInterval(tmrUpdWifi);
				_wifiScanCache = data.wifi || [];
				renderWifiList();
			}).catch(() => { if (++polls >= 15) expire(); });
		}, 2000);
	}).catch(giveUp);
}

// Masked text input where the engine supports -webkit-text-security (Webkit/
// Blink); a real password input otherwise (Firefox). Avoids browser password
// managers latching onto the wifi join form. The field starts readonly and is
// unlocked on first interaction - Safari/Chrome evaluate autofill (incl. the
// strong-password suggestion) on page load, before the user can type.
const _wifiPwType = (window.CSS && CSS.supports && CSS.supports("-webkit-text-security", "disc")) ? "text" : "password";
const _wifiPwInput = '<input type="' + _wifiPwType + '" class="form-control form-control-sm' + (_wifiPwType === "text" ? " wifi-pw" : "") + '" placeholder="Password" autocomplete="' + (_wifiPwType === "text" ? "off" : "new-password") + '" readonly />';

// Unlock a readonly-anti-autofill input on first pointer/focus interaction.
function wifiUnlockInput(input) {
	const unlock = () => { input.readOnly = false; };
	input.addEventListener("pointerdown", unlock, { once: true });
	input.addEventListener("focus", unlock, { once: true });
	input.addEventListener("input", () => input.classList.remove("is-invalid"));
}

// Turn a raw dBm value into 0-3 iOS-style signal bars.
function wifiSignalBars(rssi) {
	const level = rssi >= -55 ? 3 : rssi >= -67 ? 2 : rssi >= -78 ? 1 : 0;
	let bars = "";
	for (let i = 1; i <= 3; i++) {
		bars += '<span class="bar b' + i + (i <= level ? " on" : "") + '"></span>';
	}
	return '<span class="wifi-signal">' + bars + '</span>';
}

// Map the ESP32 encryption type (wifi_auth_mode_t) to a friendly label.
function wifiSecurityLabel(secure) {
	switch (secure) {
		case 2: return "WPA";
		case 3: case 4: return "WPA2";
		case 5: return "WEP";
		case 7: return "OPEN";
		case 8: return "AUTO";
		default: return "UNKNOWN";
	}
}

const _wifiLockSvg = '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><rect x="3" y="7.5" width="10" height="6.5" rx="1.5"/><path d="M5.5 7.5V5.5a2.5 2.5 0 0 1 5 0v2"/></svg>';
const _wifiOpenSvg = '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="6"/><path d="M8 7v3.5"/><circle cx="8" cy="5.2" r="0.9" fill="currentColor" stroke="none"/></svg>';
const _wifiChevSvg = '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" style="width:14px;height:14px"><path d="M6 3.5 10.5 8 6 12.5"/></svg>';
const _wifiCheckSvg = '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="width:15px;height:15px"><path d="M3 8.5 6.5 12 13 4.5"/></svg>';

// "Join Other Network…" row: always the last entry of the network list.
const _wifiJoinRow = '<div class="wifi-item" id="wifiJoinOtherRow" onclick="wifiShowOther()">' +
	'<div class="wifi-row-main">' +
		'<span class="wifi-plus" aria-hidden="true">+</span>' +
		'<span class="wifi-name">Join Other Network…</span>' +
		'<span class="wifi-chev">' + _wifiChevSvg + '</span>' +
	'</div></div>';

// Last scan results, cached so forget/join can re-render without rescanning.
let _wifiScanCache = [];

// Security of the saved network: set on join, else looked up in the scan
// cache; defaults to secured (the overwhelmingly common case).
let _wifiSavedSecure = null;

function wifiSavedSecure() {
	const saved = wifiSavedSsid();
	if (_wifiSavedSecure !== null && saved) return _wifiSavedSecure;
	const hit = _wifiScanCache.find(n => n.ssid === saved);
	return hit ? hit.secure !== 7 : true;
}

// The SSID currently saved on the device (filled by data-replace on load).
function wifiSavedSsid() {
	return ((getEl("WIFISSID") || {}).value || "").trim();
}

// Build the FormData the backend expects for the wifi page save.
function wifiFormData() {
	const form = getQ("form.saveParams");
	const fd = form ? new FormData(form) : new FormData();
	fd.set("pageId", String(pages.API_PAGE_WIFI.num));
	return fd;
}

// Join a network: POST the target creds, then poll the connect state. The
// backend only commits the config after a successful connection (commit-on-
// connect), so the saved-network UI is updated only once the link is really up.
function wifiBusyBtn(btn, busy) {
	if (btn) btn.disabled = busy;
}

// Sequence token: a fresh join invalidates the poll loop of a previous one,
// so a stale poll can't overwrite the newer attempt's status.
let _wifiJoinSeq = 0;

function wifiJoin(ssid, pass, statusEl, btn, secure) {
	const seq = ++_wifiJoinSeq;
	const fd = wifiFormData();
	fd.set("WIFISSID", ssid);
	fd.set("WIFIpassword", pass);
	if (secure !== undefined) _wifiSavedSecure = !!secure;
	if (statusEl) { statusEl.className = "wifi-join-status connecting"; statusEl.textContent = "Connecting…"; }
	wifiBusyBtn(btn, true);
	fetch("/saveParams", { method: "POST", body: fd }).catch(() => {
		if (seq !== _wifiJoinSeq) return;
		if (statusEl) { statusEl.className = "wifi-join-status err"; statusEl.textContent = "Save failed. Check your network."; }
		wifiBusyBtn(btn, false);
	});
	wifiPollConnect(seq, statusEl, ssid, pass, btn);
}

// Poll the STA connect state until it resolves, then refresh the UI.
function wifiPollConnect(seq, statusEl, ssid, pass, btn) {
	let tries = 0;
	const tmr = setInterval(() => {
		if (seq !== _wifiJoinSeq) { clearInterval(tmr); return; }
		fetch(apiLink + api.actions.API_WIFICONNECTSTAT).then(r => r.json()).then(data => {
			if (seq !== _wifiJoinSeq) { clearInterval(tmr); return; }
			tries++;
			if (data.connected) {
				clearInterval(tmr);
				wifiBusyBtn(btn, false);
				if (statusEl) { statusEl.className = "wifi-join-status ok"; statusEl.textContent = "Connected"; }
				const si = getEl("WIFISSID");
				const pi = getEl("WIFIpassword");
				if (si) si.value = ssid;
				if (pi) pi.value = pass;
				// Join-other succeeded: fold its form back up.
				if (statusEl && statusEl.id === "wifiOtherStatus") {
					toggleCollapse("wifiOtherForm", false);
					const row = getEl("wifiJoinOtherRow");
					if (row) row.classList.remove("open");
					["otherSsid", "otherPass"].forEach(id => { const e = getEl(id); if (e) e.value = ""; });
				}
				renderWifiYourNetwork();
				renderWifiList();
			} else if (tries >= 15) {
				clearInterval(tmr);
				wifiBusyBtn(btn, false);
				if (statusEl) { statusEl.className = "wifi-join-status err"; statusEl.textContent = "Could not connect. Check the password."; }
			}
		}).catch(() => {
			if (seq !== _wifiJoinSeq) { clearInterval(tmr); return; }
			if (++tries >= 15) {
				clearInterval(tmr);
				wifiBusyBtn(btn, false);
				if (statusEl) { statusEl.className = "wifi-join-status err"; statusEl.textContent = "Connection status unavailable."; }
			}
		});
	}, 1000);
}

// Show the saved network (if any) in the "Your Network" section.
function renderWifiYourNetwork() {
	const saved = wifiSavedSsid();
	const row = getEl("wifiYourRow");
	const empty = getEl("wifiYourEmpty");
	if (!row || !empty) return;
	if (!saved) {
		row.style.display = "none";
		empty.style.display = "";
		return;
	}
	row.style.display = "";
	empty.style.display = "none";
	const name = getEl("wifiYourName");
	if (name) name.textContent = saved;
	const sig = getEl("wifiYourSignal");
	if (sig) sig.innerHTML = wifiSignalBars(-40); // connected/remembered = full bars
	const check = getEl("wifiYourCheck");
	if (check) check.innerHTML = _wifiCheckSvg;
	const sec = getEl("wifiYourSec");
	if (sec) sec.innerHTML = wifiSavedSecure() ? _wifiLockSvg : _wifiOpenSvg;
}

// Render the scanned networks as an iOS-style list (from _wifiScanCache).
// Dedupe by SSID, sort strongest first; the saved network keeps a checkmark.
function renderWifiList() {
	const wifiTable = getEl("wifiList");
	if (!wifiTable) return;
	wifiTable.innerHTML = "";

	if (!_wifiScanCache || _wifiScanCache.length === 0) {
		wifiTable.innerHTML = '<div class="wifi-item wifi-placeholder"><span class="wifi-name">No Wi-Fi networks found.</span></div>' + _wifiJoinRow;
		return;
	}

	const bySsid = {};
	_wifiScanCache.forEach(n => {
		if (!n.ssid) return;
		const key = n.ssid;
		if (!bySsid[key] || n.rssi > bySsid[key].rssi) bySsid[key] = n;
	});

	const saved = wifiSavedSsid();
	const sorted = Object.keys(bySsid)
		.map(k => bySsid[k])
		.sort((a, b) => b.rssi - a.rssi);

	sorted.forEach(n => {
		if (saved && n.ssid === saved) return; // already shown in "Your Network"
		const open = (n.secure === 7);
		const item = document.createElement("div");
		item.className = "wifi-item";

		const secIcon = open
			? '<span class="wifi-open" title="Open network">' + _wifiOpenSvg + '</span>'
			: '<span class="wifi-lock" title="Protected · ' + wifiSecurityLabel(n.secure) + '">' + _wifiLockSvg + '</span>';
		const currentMark = "";

		item.innerHTML =
			'<div class="wifi-row-main">' +
				wifiSignalBars(n.rssi) +
				'<span class="wifi-name"></span>' +
				currentMark +
				secIcon +
				'<span class="wifi-chev">' + _wifiChevSvg + '</span>' +
			'</div>' +
			'<div class="wifi-connect">' +
				(open ? "" : _wifiPwInput) +
				'<button type="button" class="btn btn-outline-primary btn-sm wifi-join">' + (open ? "Join" : "Connect") + '</button>' +
				'<span class="wifi-join-status"></span>' +
			'</div>';

		item.querySelector(".wifi-name").textContent = n.ssid;

		item.addEventListener("click", function (e) {
			if (e.target.closest(".wifi-connect")) return;
			getQA(".wifi-item.open").forEach(o => { if (o !== item) o.classList.remove("open"); });
			item.classList.toggle("open");
			const pass = item.querySelector(".wifi-connect input");
			if (pass && item.classList.contains("open")) pass.focus();
		});

		item.querySelector(".wifi-join").addEventListener("click", function () {
			const passInput = item.querySelector(".wifi-connect input");
			const pass = passInput ? passInput.value : "";
			const statusEl = item.querySelector(".wifi-join-status");
			if (!open && pass.length < 8) {
				if (passInput) passInput.classList.add("is-invalid");
				if (statusEl) { statusEl.className = "wifi-join-status err"; statusEl.textContent = "Password must be at least 8 characters."; }
				return;
			}
			wifiJoin(n.ssid, pass, statusEl, this, !open);
		});

		const passInput = item.querySelector(".wifi-connect input");
		if (passInput) {
			wifiUnlockInput(passInput);
			passInput.addEventListener("keydown", e => {
				if (e.key === "Enter") { e.preventDefault(); item.querySelector(".wifi-join").click(); }
			});
		}

		wifiTable.appendChild(item);
	});

	wifiTable.insertAdjacentHTML("beforeend", _wifiJoinRow);
}

function toggleCollapseIp(show) {
	const panel = getEl("wifiIpPanel");
	if (panel) panel.style.display = show ? "" : "none";
}

// Fetch the live STA connection info (ip/mask/gw) once; used by the info panel
// and to reflect connected state in "Your Network".
function wifiRefreshConnection() {
	return fetch(apiLink + api.actions.API_WIFICONNECTSTAT)
		.then(r => r.json())
		.then(data => { _wifiConnInfo = data; return data; })
		.catch(() => { _wifiConnInfo = null; return null; });
}
let _wifiConnInfo = null;

function wifiToggleInfo() {
	const panel = getEl("wifiIpPanel");
	if (!panel) return;
	const showing = panel.style.display !== "none";
	toggleCollapseIp(!showing);
	const row = getEl("wifiYourRow");
	if (row) row.classList.toggle("info-open", !showing);
	if (!showing) {
		const dhcp = getEl("dhcpWiFi");
		if (dhcp) WifiDhcpDsbl(dhcp.checked);
	}
}

function setText(id, txt) { const e = getEl(id); if (e) e.textContent = txt; }

// DHCP switch: DHCP on → show live values (or an "asleep" note when WiFi is
// not the active link); off → show the static IP inputs. Inputs are always
// read-only while DHCP is on.
function WifiDhcpDsbl(state) {
	const cur = getEl("wifiIpCurrent");
	const stat = getEl("wifiIpStatic");
	if (cur) cur.style.display = state ? "" : "none";
	if (stat) stat.style.display = state ? "none" : "";
	["WifiIp", "WifiMask", "WifiGateway"].forEach(id => { const e = getEl(id); if (e) e.disabled = state; });
	if (state) {
		wifiRefreshConnection().then(d => {
			const note = getEl("wifiIpAsleep");
			const vals = getEl("wifiIpCurrentValues");
			if (d && d.connected) {
				if (note) note.style.display = "none";
				if (vals) vals.style.display = "";
				setText("wifiCurIp", d.ip || "-");
				setText("wifiCurMask", d.mask || "-");
				setText("wifiCurGw", d.gw || "-");
			} else {
				// WiFi is asleep (Ethernet primary) or still connecting: no live
				// values to show - say so instead of printing "-".
				if (note) note.style.display = "";
				if (vals) vals.style.display = "none";
			}
		});
	}
}

// Save the IP settings inline (no modal). Keeps the current saved network.
function wifiSaveIp() {
	const fd = wifiFormData();
	const statusEl = getEl("wifiIpStatus");
	if (statusEl) { statusEl.className = "wifi-ip-status"; statusEl.textContent = "Saving…"; }
	fetch("/saveParams", { method: "POST", body: fd }).then(() => {
		if (statusEl) { statusEl.className = "wifi-ip-status ok"; statusEl.textContent = "Saved"; }
	}).catch(() => {
		if (statusEl) { statusEl.className = "wifi-ip-status err"; statusEl.textContent = "Save failed. Check your network."; }
	});
}

// Forget the saved network: clear config, disconnect STA, update the UI.
function wifiForget() {
	if (!confirm("Forget this Wi-Fi network?")) return;
	const ssidInput = getEl("WIFISSID");
	const pwdInput = getEl("WIFIpassword");
	if (ssidInput) ssidInput.value = "";
	if (pwdInput) pwdInput.value = "";
	_wifiSavedSecure = null;
	const fd = wifiFormData();
	const statusEl = getEl("wifiIpStatus");
	if (statusEl) { statusEl.className = "wifi-ip-status"; statusEl.textContent = "Forgetting…"; }
	fetch("/saveParams", { method: "POST", body: fd }).then(() => {
		if (statusEl) { statusEl.className = "wifi-ip-status ok"; statusEl.textContent = "Network forgotten."; }
		toggleCollapseIp(false);
		renderWifiYourNetwork();
		renderWifiList();   // drop the checkmark from the list too
	}).catch(() => {
		if (statusEl) { statusEl.className = "wifi-ip-status err"; statusEl.textContent = "Failed to forget network."; }
	});
}

function wifiShowOther() {
	const form = getEl("wifiOtherForm");
	const wasOpen = form && form.classList.contains("show");
	toggleCollapse("wifiOtherForm", !wasOpen);
	const row = getEl("wifiJoinOtherRow");
	if (row) row.classList.toggle("open", !wasOpen);
	if (!wasOpen) {
		const other = getEl("otherSsid");
		if (other) other.focus();
	}
}

function wifiJoinOther() {
	const statusEl = getEl("wifiOtherStatus");
	const ssid = ((getEl("otherSsid") || {}).value || "").trim();
	const pass = (getEl("otherPass") || {}).value || "";
	const openNet = (getEl("otherSec") || {}).value === "open";
	if (ssid.length < 2) {
		const other = getEl("otherSsid");
		if (other) other.classList.add("is-invalid");
		if (statusEl) { statusEl.className = "wifi-join-status err"; statusEl.textContent = "Enter a network name."; }
		return;
	}
	if (!openNet && pass.length < 8) {
		const other = getEl("otherPass");
		if (other) other.classList.add("is-invalid");
		if (statusEl) { statusEl.className = "wifi-join-status err"; statusEl.textContent = "Password must be at least 8 characters."; }
		return;
	}
	wifiJoin(ssid, openNet ? "" : pass, statusEl, getQ("#wifiOtherForm .btn"), !openNet);
}

// iOS-like auto behaviour: render the saved network immediately and start a
// scan the moment the page loads (no need to press a scan button).
function initWifiPage() {
	renderWifiYourNetwork();
	getWifiList();
}

function isMobile() {
	return window.innerWidth <= 767;
}

function sidenavAutoclose(now) {
	const sidenav = getEl("sidenav");
	if (now) { if (sidenav) sidenav.classList.remove("sidenav-active"); }
	else { setTimeout(() => { if (sidenav) sidenav.classList.remove("sidenav-active"); }, 5000); }
}

function setupSwipeHandler() {
	document.addEventListener("touchstart", handleSwipe, false);
	document.addEventListener("touchend", handleSwipe, false);
	var startPoint;
	function handleSwipe(event) {
		const sidenav = getEl("sidenav");
		if (event.type == "touchend") {
			let endPoint = event.changedTouches[0].clientX;
			if (startPoint < 30 && (endPoint - startPoint) > 50) {
				if (sidenav) sidenav.classList.add("sidenav-active");
				sidenavAutoclose();
			} else if (endPoint == startPoint) {
				// tap: only close the drawer when tapping outside of it
				const t = event.changedTouches[0];
				const el = document.elementFromPoint(t.clientX, t.clientY);
				if (sidenav && el && !sidenav.contains(el)) sidenav.classList.remove("sidenav-active");
			}
		} else {
			startPoint = event.touches[0].clientX;
		}
	}
}

function KeepWebDsbl(state) {
	const e = getEl("keepWeb"); if (e) e.disabled = state;
}

function EthInputDsbl(state) {
	["EthIp", "EthMask", "EthGateway"].forEach(id => { const e = getEl(id); if (e) e.disabled = state; });
	const div = getEl("div_show3"); if (div) div.style.display = state ? "none" : "";
}

function MqttInputDsbl(state) {
	["MqttServer", "MqttPort", "MqttUser", "MqttPass", "MqttTopic", "MqttInterval", "MqttDiscovery"].forEach(id => { const e = getEl(id); if (e) e.disabled = state; });
	const div = getEl("div_show4"); if (div) div.style.display = state ? "none" : "";
}

function WgInputDsbl(state) {
	["WgLocalAddr", "WgLocalPrivKey", "WgEndAddr", "WgEndPubKey", "WgEndPort"].forEach(id => { const e = getEl(id); if (e) e.disabled = state; });
	const div = getEl("div_show5"); if (div) div.style.display = state ? "none" : "";
}

let statusRefreshTimer = null;
const statusRefreshKeys = ["z2mConnected", "z2mClientIp", "connectedSocket", "zbStatus", "zbAutoResets", "connectedSocketStatus", "uptime"];

// Live-refresh of the status page: poll /status (JSON of getRootData) and update
// the data-replace cells in place, without reloading the whole page.
function refreshStatusData() {
	if (!getQ("[data-replace='z2mLink']")) {
		if (statusRefreshTimer) { clearInterval(statusRefreshTimer); statusRefreshTimer = null; }
		return;
	}
	fetch("/status", { cache: "no-store" }).then(r => r.json()).then(values => {
		statusRefreshKeys.forEach(key => {
			if (values[key] === undefined) return;
			const valueToSet = String(values[key]);
			const isIpValue = /^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$/.test(valueToSet);
			getQA("[data-replace='" + key + "']").forEach(elem => {
				const elemType = elem.nodeName.toLowerCase();
				if (elemType === "input" || elemType === "select" || elemType === "textarea" || elemType === "option") return;
				if (isIpValue) {
					elem.innerHTML = '<a href="http://' + valueToSet + '">' + valueToSet + '</a>';
				} else {
					const pill = statusPill(valueToSet);
					if (pill) elem.innerHTML = pill;
					else elem.textContent = valueToSet;
				}
			});
		});
	}).catch(() => {});
}

function startStatusRefresh(intervalMs) {
	if (statusRefreshTimer) clearInterval(statusRefreshTimer);
	statusRefreshTimer = setInterval(refreshStatusData, intervalMs);
}

function statusPill(value) {
	const s = String(value).trim().toLowerCase();
	let tone = null;
	if (s === "yes" || s === "on" || s === "connected" || s.indexOf("connected to") === 0) tone = "success";
	else if (s === "set up" || s === "asleep") tone = "info";
	else if (s.indexOf("connecting") === 0) tone = "warning";
	else if (s === "no" || s === "off" || s === "not connected" || s === "not started" || s === "disconnected" || s === "not configured" || s === "ap not started") tone = "secondary";
	if (!tone) return null;
	return '<span class="status-pill ' + tone + '">' + String(value) + '</span>';
}

function toggleWgKeyVisibility() {
	toggleInputVisibility("WgLocalPrivKey");
}

function toggleInputVisibility(id) {
	const input = getEl(id);
	if (input) input.type = input.type === "password" ? "text" : "password";
}

function SeqInputDsbl(state) {
	["webUser", "webPass"].forEach(id => { const e = getEl(id); if (e) e.disabled = state; });
	const div = getEl("div_show1"); if (div) div.style.display = state ? "none" : "";
}

function SeqInputDsblFw(state) {
	const e = getEl("fwIp"); if (e) e.disabled = state;
	const div = getEl("div_show2"); if (div) div.style.display = state ? "none" : "";
}

// ---- ZNP frame humanizer (display only; the raw bytes stay in the log) ----
const znpSubs = ["RES", "SYS", "MAC", "NWK", "AF", "ZDO", "SAPI", "UTIL", "DEBUG", "APP", "APP_CNF"];
const znpTypes = ["POLL", "SREQ", "AREQ", "SRSP"];

function znpHex(n, w) { return "0x" + Number(n).toString(16).toUpperCase().padStart(w || 2, "0"); }
function znpAddr(lo, hi) { return znpHex(lo | (hi << 8), 4); }
function znpStatus(b) { return b === 0 ? "OK" : znpHex(b); }

function decodeZcl(p) {
	if (p.length < 3) return null;
	const names = { 0x00: "Read", 0x01: "ReadRsp", 0x08: "DiscoverRsp", 0x09: "Report", 0x0A: "Report", 0x0B: "DefaultRsp" };
	return "ZCL " + (names[p[2]] || znpHex(p[2]));
}

function decodeAfFrame(type, cmd, d) {
	if (type === "SREQ" && cmd === 0x01 && d.length >= 10)
		return "AF_DATA_REQUEST → " + znpAddr(d[0], d[1]) + " · cluster " + znpHex(d[4] | (d[5] << 8), 4) + " (" + d[9] + "B)";
	if (type === "SRSP" && cmd === 0x01 && d.length >= 1)
		return "AF_DATA_REQUEST_RSP status=" + znpStatus(d[0]);
	if (type === "AREQ" && cmd === 0x80 && d.length >= 3)
		return "AF_DATA_CONFIRM ep " + d[1] + " · status=" + znpStatus(d[0]);
	if (type === "AREQ" && cmd === 0x81 && d.length >= 17) {
		let s = "AF_INCOMING_MSG from " + znpAddr(d[4], d[5]) + " · cluster " + znpHex(d[2] | (d[3] << 8), 4) + " · LQI " + d[9];
		const zcl = decodeZcl(d.slice(17));
		if (zcl) s += " · " + zcl;
		return s;
	}
	return null;
}

function znpGenericName(c0, c1) {
	return znpSubs[c0 & 0x1F] + " " + znpTypes[(c0 >> 5) & 7] + " " + znpHex(c1);
}

// Exact-total frame lengths (incl SOF+LEN+FCS) for commands we fully decode.
// Returns {text, next} or null. d = data bytes at/after the command.
function znpTry(t, i, hasSof) {
	const b = hasSof ? 2 : 1;
	if (t.length - i < b + 3) return null;
	const c0 = t[i + b], c1 = t[i + b + 1];
	if (((c0 >> 5) & 7) > 3 || !znpSubs[c0 & 0x1F]) return null;
	const d = t.slice(i + b + 2);
	const type = znpTypes[(c0 >> 5) & 7];
	let need, text = null;
	if (c0 === 0x24 && c1 === 0x01) { // AF_DATA_REQUEST
		if (d.length < 10) return null;
		need = 10 + d[9];
		text = decodeAfFrame(type, c1, d);
	} else if (c0 === 0x64 && c1 === 0x01) { // AF_DATA_REQUEST_RSP
		need = 1;
		text = decodeAfFrame(type, c1, d);
	} else if (c0 === 0x44 && c1 === 0x80) { // AF_DATA_CONFIRM
		need = 3;
		text = decodeAfFrame(type, c1, d);
	} else if (c0 === 0x44 && c1 === 0x81) { // AF_INCOMING_MSG
		if (d.length < 17) return null;
		need = 17 + d[16];
		text = decodeAfFrame(type, c1, d);
	} else if (c0 === 0x21 && c1 === 0x02) { // SYS_VERSION
		need = 1;
		text = "SYS_VERSION";
	} else if (c0 === 0x61 && c1 === 0x02) { // SYS_VERSION response
		need = 5;
		text = "SYS_VERSION v" + d[2] + "." + d[3] + "." + d[4];
	} else {
		return null;
	}
	const total = b + 2 + need + 1;
	if (t.length - i < total) return null;
	return text ? { text, next: i + total } : null;
}

// Split a token run into decodable frames + raw residue. Handles lines where
// the 8 KiB ring glued two frames together or overwrote a frame head/tail.
function znpSegments(t) {
	const segs = [];
	let i = 0, rawStart = 0;
	const flushRaw = end => { if (end > rawStart) segs.push({ raw: t.slice(rawStart, end) }); };
	const isFeStart = k => t[k] === 0xFE && t.length - k >= 5 &&
		(((t[k + 2] >> 5) & 7) <= 3) && !!znpSubs[t[k + 2] & 0x1F];
	const isKnownPair = (c0, c1) => (c0 === 0x24 && c1 === 0x01) || (c0 === 0x64 && c1 === 0x01) ||
		(c0 === 0x44 && c1 === 0x80) || (c0 === 0x44 && c1 === 0x81);
	while (i < t.length) {
		let f = null;
		if (t[i] === 0xFE && t.length - i >= 5) {
			f = znpTry(t, i, true);
			if (!f) {
				const c0 = t[i + 2], c1 = t[i + 3];
				if (((c0 >> 5) & 7) <= 3 && znpSubs[c0 & 0x1F]) {
					let end = t.length;
					for (let k = i + 1; k < t.length; k++) {
						if (isFeStart(k)) { end = k; break; }
					}
					f = { text: znpGenericName(c0, c1), next: end };
				}
			}
		} else if (i + 4 < t.length && isKnownPair(t[i + 1], t[i + 2])) {
			f = znpTry(t, i, false); // SOF lost to a ring overwrite, length still validates
		}
		if (f) { flushRaw(i); segs.push({ text: f.text }); i = f.next; rawStart = i; }
		else i++;
	}
	flushRaw(t.length);
	return segs;
}

const LOG_LINE_RE = /^\[([\d:]+)\] (<?->?)\s+(.*)$/;

function humanizeZnpLine(line) {
	const m = line.match(LOG_LINE_RE);
	if (!m) return line;
	const raw = m[3].trim().split(/\s+/);
	if (!raw.length || !raw.every(h => /^[0-9a-fA-F]{2}$/.test(h))) return line;
	const t = raw.map(h => parseInt(h, 16));
	const segs = znpSegments(t);
	if (!segs.some(s => s.text)) return line;
	const hex = a => a.map(b => ("0" + b.toString(16)).slice(-2)).join(" ");
	return "[" + m[1] + "] " + m[2] + " " + segs.map(s => s.text || hex(s.raw)).join(" + ");
}

function humanizeLog(text) {
	const hz = getEl("humanizeSw");
	if (hz && !hz.checked) return text;
	return text.split("\n").map(humanizeZnpLine).join("\n");
}

let logFilterMode = localStorage.getItem("logfilter") || "esp";
if (!["esp", "zb"].includes(logFilterMode)) logFilterMode = "esp";

function setLogFilter(mode, seg) {
	logFilterMode = mode;
	localStorage.setItem("logfilter", mode);
	if (seg) seg.querySelectorAll("button").forEach(b => b.classList.toggle("seg-active", b.dataset.logfilter === mode));
	const hw = getEl("humanizeWrap");
	if (hw) hw.style.display = mode === "zb" ? "" : "none"; // only meaningful for frames
}

// Ring debris guard: when a ring churns, fetched text can start mid-line or
// glue a truncated head onto the next line. Cut back to the last
// "[ts] <-/->/ |" line start, ignore the chunk when it has none.
function intactLogLine(line) {
	const re = /\[(\d\d:\d\d:\d\d|\d+)\] (<-|->|\|)/g;
	let m, last = -1;
	while ((m = re.exec(line))) last = m.index;
	if (last < 0) return null;
	const s = line.slice(last);
	return /^\[[^\]]+\] (<?->?)?\s*$/.test(s) ? null : s;
}

// Filter mode doubles as ring selector: esp = event ring, zb = ZNP frame ring.
function renderLogs() {
	const data = logFilterMode === "esp" ? lastEspText : lastZbText;
	return humanizeLog(data.split("\n").map(intactLogLine).filter(Boolean).join("\n"));
}

let lastEspText = "", lastZbText = "";
var logUpd = null;

function fetchLogs() {
	return Promise.all([
		fetch(apiLink + api.actions.API_GET_LOG + "&src=esp").then(r => r.text()).then(t => { lastEspText = t; }).catch(() => ""),
		fetch(apiLink + api.actions.API_GET_LOG + "&src=zb").then(r => r.text()).then(t => { lastZbText = t; }).catch(() => "")
	]);
}

function logRefresh(ms) {
	const prgZb = getEl("prg_zb");
	const barZb = getEl("bar_zb");
	if (prgZb && barZb && barZb.style.width && barZb.style.width !== "0%") {
		console.log("Pausing log refresh during firmware update");
		return;
	}
	logUpd = setInterval(() => {
		fetchLogs().then(() => {
			const consoleEl = getEl("console");
			if (consoleEl) {
				const stick = consoleEl.scrollHeight - consoleEl.scrollTop - consoleEl.clientHeight < 40;
				consoleEl.value = renderLogs();
				if (stick) consoleEl.scrollTop = consoleEl.scrollHeight;
			}
			else clearInterval(logUpd);
		});
	}, ms);
}

// Instant re-render of the last poll (no poll wait) for toggle/filter
// clicks. Always jumps to the tail: the user just changed the view.
function repaintLogs() {
	const consoleEl = getEl("console");
	if (!consoleEl || !(lastEspText + lastZbText)) return;
	consoleEl.value = renderLogs();
	consoleEl.scrollTop = consoleEl.scrollHeight;
}

async function fetchData(url, isJson = true) {
	if (isJson) {
		const separator = url.includes("?") ? "&" : "?";
		url = url + separator + "_t=" + Date.now();
		return await fetch(url).then(r => r.json());
	} else {
		return await fetch(url).then(r => r.text());
	}
}

async function processResponses() {
	try {
		let jsonUrl = `https://api.github.com/repos/${GITHUB_REPO_OWNER}/${GITHUB_REPO_NAME}/releases/latest`;
		let textUrl = "/api?action=1&param=espVer";
		let [jsonData, textData] = await Promise.all([fetchData(jsonUrl, true), fetchData(textUrl, false)]);
		return { jsonData, textData };
	} catch (error) {
		console.error("Error while getting versions:", error);
	}
}

function checkLatestESPrelease() {
	processResponses().then(combinedData => {
		var downloadCount = (combinedData.jsonData.assets || []).reduce((n, a) => n + (a.download_count || 0), 0);
		var releaseInfo = "New firmware (" + combinedData.jsonData.tag_name + ") was found on GitHub. It was downloaded " + downloadCount.toLocaleString() + " times. Go -> About -> ESP32 Firmware to update.";
		if (verIsNewer(combinedData.jsonData.tag_name, combinedData.textData)) {
			setTimeout(() => {
				if (!(localStorage.getItem("update_notify") == 1)) modalConstructor("espGitUpdate", releaseInfo);
				console.log(releaseInfo);
			}, 500);
		}
	});
}

let isZbVersionLoading = false;
let latestZbFirmware = null;

async function showZigbeeVersionInfo(firmwareType) {
	if (isZbVersionLoading) { console.log("Already loading, ignore..."); return; }
	isZbVersionLoading = true;
	try {
		modalConstructor("zbVersionInfo", { text: "Loading firmware information...", chglog: "<div class='spinner-border text-primary' role='status'></div> Loading..." });
		let fwData = null;
		let localVerData = null;
		try {
			[fwData, localVerData] = await Promise.all([fetchData(zbFwInfoUrl, true), fetchData("/api?action=1&param=zbRev", false)]);
		} catch (err) {
			console.warn("Failed to load:", err.message || err);
		}
		if (!fwData || (Array.isArray(fwData) && fwData.length === 0)) {
			const mBody = getQ(".modal-body");
			if (mBody) mBody.innerHTML = "<div class='alert alert-warning'><strong>No firmware data available.</strong><br>Please check your internet connection or try again later.</div>";
			isZbVersionLoading = false;
			return;
		}
		let selectedFw = Array.isArray(fwData) ? fwData.find(fw => {
			const name = fw.name ? fw.name.toLowerCase() : "";
			const fwType = fw.firmwareType ? fw.firmwareType.toLowerCase() : "";
			if (firmwareType === "coordinator") return name.includes("coordinator") && fwType === "zigbee";
			if (firmwareType === "router") return name.includes("router") && fwType === "zigbee";
			return false;
		}) : null;
		if (!selectedFw) {
			const mBody = getQ(".modal-body");
			if (mBody) mBody.innerHTML = "<div class='alert alert-warning'><strong>No firmware found for '" + firmwareType + "'</strong><br>Available firmware types:<br><ul>" + (Array.isArray(fwData) ? fwData.map(fw => "<li>" + (fw.name || "N/A") + " - " + (fw.firmwareType || "N/A") + "</li>").join("") : "<li>Unknown format</li>") + "</ul></div>";
			isZbVersionLoading = false;
			return;
		}
		latestZbFirmware = { ...selectedFw, localVersion: localVerData ? localVerData.trim() : "unknown" };
		const localVer = latestZbFirmware.localVersion, remoteVer = latestZbFirmware.version || "unknown";
		const hasUpdate = localVer === "unknown" || verIsNewer(remoteVer, localVer);
		const text = "Local version: v" + localVer + "<br>GitHub version: " + remoteVer + "<br>File: " + latestZbFirmware.name + "<br>Description: " + (latestZbFirmware.firmwareDesc || "N/A");
		let chglog = `<p><strong>Firmware Details:</strong></p><ul><li><strong>Name:</strong> ${latestZbFirmware.name}</li><li><strong>Version:</strong> ${latestZbFirmware.version}</li><li><strong>Type:</strong> ${latestZbFirmware.firmwareType || "N/A"}</li><li><strong>Description:</strong> ${latestZbFirmware.firmwareDesc || "N/A"}</li><li><strong>Baud Rate:</strong> ${latestZbFirmware.baudRate || "N/A"}</li><li><strong>Dongle Type:</strong> ${latestZbFirmware.dongleType || "N/A"}</li></ul>`;
		if (localVer !== "unknown" && remoteVer !== "unknown")
			chglog += hasUpdate
				? "<div class='alert alert-success mt-2'><strong>Update Available!</strong><br>Current: v" + localVer + " -> Latest: v" + remoteVer + "</div>"
				: "<div class='alert alert-secondary mt-2'><strong>Up to date</strong><br>Current version: v" + localVer + "</div>";
		modalConstructor("zbVersionInfo", { text, chglog, hasUpdate });
	} catch (error) {
		console.error("Error while getting Zigbee version info:", error);
		const mBody = getQ(".modal-body");
		if (mBody) mBody.innerHTML = "<div class='alert alert-danger'><strong>Error fetching version information.</strong><br>Please check your network connection.<br><small>" + (error.message || error) + "</small></div>";
	} finally {
		isZbVersionLoading = false;
	}
}

// Numeric version compare.
function verIsNewer(remote, local) {
	const rn = Number(String(remote).replace(/[^0-9.]/g, "").split(".").join(""));
	const ln = Number(String(local).replace(/[^0-9.]/g, "").split(".").join(""));
	return rn > 0 && rn > ln;
}

function escHtml(s) {
	return String(s == null ? "" : s).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

let espFwInfo = null;

// Details modal for the ESP32 card: installed + latest + release notes,
// available for any version (not just when an update is pending).
function showEspFwDetails() {
	if (!espFwInfo) { checkEspFirmware(); return; }
	const text = "Installed v" + espFwInfo.local + ". Latest " + espFwInfo.tag + ".";
	const chglog = "<pre>" + escHtml(espFwInfo.body || "(no release notes)") + "</pre>";
	modalConstructor("espFwDetails", { text, chglog, hasUpdate: espFwInfo.hasUpdate });
}

// ESP32 firmware status on the About page: only offers an update (and shows
// the changelog) when GitHub actually has something newer.
async function checkEspFirmware() {
	const st = getEl("espFwStatus");
	if (!st) return;
	st.innerHTML = "<span class='text-muted'>Checking for updates...</span>";
	let combined = null;
	try {
		combined = await processResponses();
	} catch (e) { console.warn("ESP32 version check failed:", e); }
	const upd = getEl("updEspGit");
	const log = getEl("espFwChglog");
	espFwInfo = null;
	if (!combined || !combined.jsonData || !combined.jsonData.tag_name) {
		st.innerHTML = "<span class='text-muted'>Could not reach GitHub. Use file upload below.</span>";
		if (upd) upd.style.display = "none";
		if (log) log.style.display = "none";
		return;
	}
	const tag = combined.jsonData.tag_name;
	const local = String(combined.textData).trim();
	const hasUpdate = verIsNewer(tag, combined.textData);
	espFwInfo = { tag, local, body: combined.jsonData.body || "", hasUpdate };
	if (hasUpdate) {
		const count = (combined.jsonData.assets || []).reduce((n, a) => n + (a.download_count || 0), 0);
		st.innerHTML = "Update available: <strong>" + tag + "</strong> (downloaded " + count.toLocaleString() + " times)";
		if (log) { log.style.display = ""; log.textContent = combined.jsonData.body || ""; }
		if (upd) upd.style.display = "";
	} else {
		st.innerHTML = "Up to date (<strong>" + escHtml(local) + "</strong>)";
		if (upd) upd.style.display = "none";
		if (log) log.style.display = "none";
	}
}

// Zigbee firmware status on the About page, same deal: per-type rows that
// only offer a flash when the indexed release is newer than the local rev.
async function checkZbFirmware() {
	const st = getEl("zbFwStatus");
	if (!st) return;
	st.innerHTML = "<span class='text-muted'>Checking for updates...</span>";
	let fwData = null, rev = "";
	try {
		[fwData, rev] = await Promise.all([fetchData(zbFwInfoUrl, true), fetchData("/api?action=1&param=zbRev", false)]);
	} catch (e) { console.warn("Zigbee version check failed:", e); }
	const localRev = (rev || "").trim();
	const box = getEl("zbFwList");
	if (!Array.isArray(fwData) || !fwData.length) {
		st.innerHTML = "<span class='text-muted'>Could not reach the firmware index. Use file upload below.</span>";
		if (box) box.innerHTML = "";
		return;
	}
	const fws = fwData.filter(fw => (fw.firmwareType || "").toLowerCase() === "zigbee");
	if (!fws.length) {
		st.innerHTML = "<span class='text-muted'>No Zigbee firmware in the index.</span>";
		if (box) box.innerHTML = "";
		return;
	}
	let newer = 0;
	if (box) {
		box.innerHTML = "";
		fws.forEach(fw => {
			const isRouter = (fw.name || "").toLowerCase().includes("router");
			const remote = fw.version || "unknown";
			const hasUpdate = verIsNewer(remote, localRev);
			if (hasUpdate) newer++;
			const row = el("div", { "class": "zb-fw-row" });
			const info = el("div", { "class": "zb-fw-info" });
			info.innerHTML = "<strong>" + (isRouter ? "Router" : "Coordinator") + "</strong>" +
				(isRouter ? "" : " <span class='default-badge'>default</span>") +
				"<div class='text-muted zb-fw-sub'>latest " + escHtml(remote) + " - " +
				(hasUpdate ? "<span style='color:#f7a623'>update available</span>" : "up to date") + "</div>";
			row.appendChild(info);
			const wrap = el("span", { "class": "zb-fw-btns" });
			wrap.appendChild(el("button", { type: "button", "class": "btn btn-outline-primary btn-sm", text: "Details", click: () => showZigbeeVersionInfo(isRouter ? "router" : "coordinator") }));
			if (hasUpdate) {
				wrap.appendChild(document.createTextNode(" "));
				wrap.appendChild(el("button", {
					type: "button", "class": "btn btn-warning btn-sm", text: "Flash " + remote, click: () => {
						if (!fw.link) return;
						latestZbFirmware = { ...fw, localVersion: localRev || "unknown" };
						startZbFlashFromGit();
					}
				}));
			}
			row.appendChild(wrap);
			box.appendChild(row);
		});
	}
	st.innerHTML = newer ? ("<span style='color:#f7a623'><strong>" + newer + " update" + (newer > 1 ? "s" : "") + " available</strong></span>")
		: "All variants up to date (local rev <strong>" + (localRev || "unknown") + "</strong>)";
}


// ========== Lightweight i18n (MicroI18n) ==========
const languages = [
	{ value: "en", text: "English", flag: "🇬🇧" },
	{ value: "zh", text: "中文", flag: "🇨🇳" },
	{ value: "de", text: "Deutsch", flag: "🇩🇪" }
];

class MicroI18n {
	constructor(lang = "en") {
		this.locale = languages.some((l) => l.value === lang) ? lang : "en";
		this.vocabulary = {
			en: { "l.st": "Status", "l.ge": "General", "l.ne": "Ethernet", "l.zi": "Z2M/ZHA", "l.se": "Security", "l.mq": "MQTT", "l.vp": "WireGuard", "l.to": "Tools", "l.ab": "About", "l.lo": "Logout" },
			zh: { "l.st": "状态", "l.ge": "通用", "l.ne": "以太网", "l.zi": "Z2M/ZHA", "l.se": "安全", "l.mq": "MQTT", "l.vp": "WireGuard", "l.to": "工具", "l.ab": "关于", "l.lo": "退出" },
			de: { "l.st": "Status", "l.ge": "Allgemein", "l.ne": "Ethernet", "l.zi": "Z2M/ZHA", "l.se": "Sicherheit", "l.mq": "MQTT", "l.vp": "WireGuard", "l.to": "Werkzeuge", "l.ab": "Über", "l.lo": "Abmelden" }
		};
	}
	t(key, vars = {}) {
		let text = (this.vocabulary[this.locale] && this.vocabulary[this.locale][key]) || this.vocabulary.en[key] || key;
		Object.keys(vars).forEach((v) => { text = text.replace(`\${${v}}`, vars[v]); });
		return text;
	}
	setLanguage(lng) { if (languages.some((l) => l.value === lng)) this.locale = lng; }
}

let microI18n;

function initLanguage() {
	const savedLang = localStorage.getItem("selected-lang");
	const browserLang = (navigator.language || navigator.userLanguage || "").substring(0, 2);
	const preferredLang = savedLang || (languages.some((l) => l.value === browserLang) ? browserLang : "en");
	microI18n = new MicroI18n(preferredLang);
	updateLangFlags(preferredLang);
	updateLocalizedContent();
}

function setLanguage(lng) {
	if (!languages.some((l) => l.value === lng)) return;
	localStorage.setItem("selected-lang", lng);
	microI18n.setLanguage(lng);
	updateLangFlags(lng);
	updateLocalizedContent();
}

function setLanguageFlag(lng) {
	setLanguage(lng);
	closeLangDropdowns();
}

function updateLangFlags(lng) {
	const match = languages.find((l) => l.value === lng);
	if (match) {
		const btnFlag = document.getElementById("langBtnFlag");
		if (btnFlag) btnFlag.textContent = match.flag;
		const btnFlagSide = document.getElementById("langBtnFlagSide");
		if (btnFlagSide) btnFlagSide.textContent = match.flag;
		const btnLabelSide = document.getElementById("langBtnLabelSide");
		if (btnLabelSide) btnLabelSide.textContent = match.text;
	}
	document.querySelectorAll("#langMenu .lang-option").forEach((opt) => {
		opt.classList.toggle("active", opt.dataset.lang === lng);
	});
	document.querySelectorAll("#langMenuSide .lang-option-side").forEach((opt) => {
		opt.classList.toggle("active", opt.dataset.lang === lng);
	});
}

function toggleLangDropdown() {
	const w = document.getElementById("langSwitcher");
	if (w) w.classList.toggle("open");
}

function toggleLangDropdownSide() {
	const w = document.getElementById("langSwitcherSide");
	if (w) w.classList.toggle("open");
}

function closeLangDropdowns() {
	["langSwitcher", "langSwitcherSide"].forEach(id => {
		const w = document.getElementById(id);
		if (w) w.classList.remove("open");
	});
}

document.addEventListener("click", function (e) {
	["langSwitcher", "langSwitcherSide"].forEach(id => {
		const w = document.getElementById(id);
		if (w && !w.contains(e.target)) w.classList.remove("open");
	});
});

function localize() {
	if (!microI18n) return;
	const elements = document.querySelectorAll("[data-i18n]");
	elements.forEach((element) => {
		const keys = element.getAttribute("data-i18n").split(";");
		keys.forEach((key) => {
			if (key.trim()) {
				if (key.includes("[")) {
					const parts = key.split("[");
					const attr = parts[1].slice(0, -1);
					element.setAttribute(attr, microI18n.t(parts[0]));
				} else {
					const inputChild = element.querySelector("input");
					if (inputChild) {
						while (element.firstChild !== inputChild) element.removeChild(element.firstChild);
						const textNode = document.createTextNode(microI18n.t(key));
						element.insertBefore(textNode, inputChild);
					} else {
						element.textContent = microI18n.t(key);
					}
				}
			}
		});
	});
}

function localizeTitle(pathname) {
	if (!microI18n) return;
	for (let key in pages) {
		if (pages[key].str === pathname) {
			const titleKey = pages[key].titleKey;
			if (titleKey) {
				document.title = microI18n.t(titleKey);
				const pn = document.getElementById("pagenamePC");
				if (pn) pn.textContent = microI18n.t(titleKey);
			}
			break;
		}
	}
}

function updateLocalizedContent() {
	localizeTitle(window.location.pathname);
	localize();
}
