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

const zbFwReleasesUrl = "https://api.github.com/repos/Koenkk/Z-Stack-firmware/releases?per_page=100";
const zbFwIncludePrereleaseKey = "zb_include_prerelease";

const GITHUB_REPO_OWNER = "AVATTO-smart";
const GITHUB_REPO_NAME = "avatto-gw90-firmware";

const pages = {
	API_PAGE_ROOT: { num: 0, str: "/", titleKey: "l.st" },
	API_PAGE_GENERAL: { num: 1, str: "/general", titleKey: "l.ge" },
	API_PAGE_ETHERNET: { num: 2, str: "/ethernet", titleKey: "l.ne" },
	API_PAGE_WIFI: { num: 3, str: "/wifi", titleKey: "l.wf" },
	API_PAGE_SERIAL: { num: 4, str: "/zha-z2m", titleKey: "l.zi" },
	API_PAGE_SECURITY: { num: 5, str: "/security", titleKey: "l.se" },
	API_PAGE_SYSTOOLS: { num: 6, str: "/sys-tools", titleKey: "l.to" },
	API_PAGE_ABOUT: { num: 7, str: "/about", titleKey: "l.ab" },
	API_PAGE_MQTT: { num: 8, str: "/mqtt", titleKey: "l.mq" },
	API_PAGE_WG: { num: 9, str: "/wg", titleKey: "l.vp" }
};
const TITLE_SUFFIX = " - AVATTO-GW90-Ti Zigbee Ethernet POE USB Adapter";

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
			apiGetPage(api.pages.API_PAGE_ABOUT, () => { initZbFirmwareSource(); checkEspFirmware(); checkZbFirmware(); });
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
						alert(t("d.saveErr"));
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
							alert(t("d.netErr"));
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

		getQA("[data-replace='pageName']").forEach(e => e.textContent = microI18n.t(page.titleKey));
		const titleEl = document.querySelector("title[data-replace='pageName']");
		if (titleEl) titleEl.textContent = microI18n.t(page.titleKey) + TITLE_SUFFIX;

			if (respValues === null) return;
			console.log("[apiGetPage] starting parse values");
			const values = JSON.parse(respValues);
			let selectedTimeZone = values.timeZoneName || null;
			fillReplaceValues(values);

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

			updateLocalizedContent();

		if (typeof locCall == "function") locCall();
		}).catch(err => {
			alert(t("d.pageErr") + err.message);
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
	document.documentElement.setAttribute("data-theme", dark ? "dark" : "light");
	localStorage.setItem("theme", dark ? "dark" : "light");
}

function toggleDarkModeMobile() {
	applyTheme(document.documentElement.getAttribute("data-theme") !== "dark");
}

function initDarkModeToggle() {
	const saved = localStorage.getItem("theme");
	const prefersDark = window.matchMedia("(prefers-color-scheme: dark)").matches;
	const isDark = saved !== null ? saved === "dark" : prefersDark;
	document.documentElement.setAttribute("data-theme", isDark ? "dark" : "light");
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
		if (prg) prg.innerHTML = t("d.githubStart");
	}, 500);
}

function zbFlashGitWait() {
	if (!latestZbFirmware) return;
	console.log("Starting Zigbee update from Git... Please wait patiently!");
	startZbFlashFromGit();
}

function normalizeZbRole(role) {
	const value = String(role || "").trim().toLowerCase();
	return value === "coordinator" || value === "router" ? value : "unknown";
}

function zbRoleLabel(role) {
	if (role === "unknown") return t("d.unknown");
	return role === "router" ? t("d.router") : t("d.coord");
}

function requestZigbeeFlash(firmware, localRole, localVersion) {
	if (!firmware || !firmware.link || localRole === "unknown") return;

	const begin = () => {
		latestZbFirmware = { ...firmware, localRole, localVersion: localVersion || "unknown" };
		startZbFlashFromGit();
	};
	if (firmware.role !== localRole) {
		modalConstructor("zbRoleSwitchConfirm", {
			currentRole: zbRoleLabel(localRole),
			targetRole: zbRoleLabel(firmware.role),
			onConfirm: begin
		});
		return;
	}
	begin();
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
		if (prgZb) prgZb.innerHTML = t("d.prepDev");
		if (barZb) barZb.style.width = "0%";
		if (updBtnZb) updBtnZb.disabled = true;
		ZBfwStartEvents();
		setTimeout(() => {
			let firmwareUrl = latestZbFirmware.link;
			console.log("Firmware URL:", firmwareUrl);
			if (!firmwareUrl) {
				console.error("Firmware URL is empty!");
				if (prgZb) prgZb.innerHTML = '<span style="color:red">' + t("d.invalidLink") + "</span>";
				if (updBtnZb) updBtnZb.disabled = false;
				return;
			}
			startFlashProcess(firmwareUrl, latestZbFirmware.role, latestZbFirmware.localRole !== latestZbFirmware.role);
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

function startFlashProcess(fwUrl, role, roleSwitch) {
	console.log("Starting flash process with URL:", fwUrl);
	const prgZb = getEl("prg_zb");
	const barZb = getEl("bar_zb");
	if (prgZb) prgZb.innerHTML = t("d.dlFw");
	if (barZb) barZb.style.width = "5%";
	let apiUrl = apiLink + api.actions.API_FLASH_ZB + "&fwurl=" + encodeURIComponent(fwUrl) + "&role=" + encodeURIComponent(role || "");
	if (roleSwitch) apiUrl += "&confirmRoleSwitch=1";
	fetch(apiUrl).then(async r => {
		const data = await r.text();
		if (!r.ok) throw new Error(data || r.statusText);
		console.log("API Response:", data);
	}).catch(err => {
		console.error("Download failed:", err);
		if (prgZb) prgZb.innerHTML = '<span style="color:red">' + t("d.dlFail") + err + "</span>";
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
		if (prg) prg.innerHTML = t("d.progress") + Math.round(e.data) + "%";
		if (bar) bar.style.width = Math.round(e.data) + "%";
		if (Math.round(e.data) > 99.5) {
			setTimeout(() => {
				if (prg) prg.innerHTML = t("d.updDoneReboot");
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
		if (prgZb) prgZb.innerHTML = '<span style="color:orange">' + t("d.evTimeout") + "</span>";
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
			if (prgZb) prgZb.innerHTML = '<span style="color:orange">' + t("d.connFailPoll") + "</span>";
			if (zbEventSource) { zbEventSource.close(); zbEventSource = null; }
		}
	});
	zbEventSource.addEventListener("ZB_FW_downloading", e => {
		const pct = parseInt(e.data);
		console.log("Downloaded: " + pct + "%");
		const prgZb = getEl("prg_zb");
		const barZb = getEl("bar_zb");
		if (prgZb) prgZb.innerHTML = t("d.downloaded") + pct + "%";
		if (barZb) barZb.style.width = pct + "%";
		if (pct >= 100) {
			if (prgZb) prgZb.innerHTML = t("d.dlComplete");
			if (barZb) barZb.style.width = "100%";
		}
	});
	zbEventSource.addEventListener("ZB_FW_flashing", e => {
		const pct = Math.min(parseInt(e.data), 100);
		console.log("Flashing progress: " + pct + "%");
		const prgZb = getEl("prg_zb");
		const barZb = getEl("bar_zb");
		if (prgZb) prgZb.innerHTML = t("d.flashing") + pct + "%";
		if (barZb) barZb.style.width = pct + "%";
		if (pct >= 100) {
			setTimeout(() => {
				if (prgZb) prgZb.innerHTML = '<span style="color:green">' + t("d.flashDone") + "</span>";
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
			if (prgZb) prgZb.innerHTML = t("d.validating");
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
	const modalEl = getEl("modal");
	if (modalEl) {
		const dismissible = !["rebootWait", "espFlashGitWait", "saveOk"].includes(type);
		modalEl.setAttribute("data-bs-backdrop", dismissible ? "true" : "static");
		modalEl.setAttribute("data-bs-keyboard", dismissible ? "true" : "false");
	}
	const hText = getQ(".modal-title");
	const mBody = getQ(".modal-body");
	const mBtns = getQ(".modal-footer");
	if (hText) { hText.textContent = ""; hText.style.color = ""; }
	if (mBody) { mBody.textContent = ""; mBody.style.color = ""; }
	if (mBtns) mBtns.innerHTML = "";
	switch (type) {
		case "flashWarning":
			if (hText) { hText.textContent = t("md.warn"); hText.style.color = "red"; }
			if (mBody) { mBody.textContent = t("md.warnBody"); mBody.style.color = "red"; }
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-success", text: t("c.close"), click: closeModal }));
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-danger", text: t("md.agree"), click: () => { closeModal(); localStorage.setItem("flash_warning", 1); const ub = getEl("updButton"); if (ub) ub.removeAttribute("disabled"); } }));
			}
			break;
		case "confirmRestart":
			if (hText) hText.textContent = t("md.rstEsp");
			if (mBody) mBody.textContent = t("md.rstBody");
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-outline-secondary", text: t("c.cancel"), click: closeModal }));
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-danger", text: t("c.restart"), click: () => { closeModal(); espReboot(); rebootWait(); } }));
			}
			break;
		case "espFwDetails":
			if (hText) hText.textContent = t("md.espFw");
			if (mBody) mBody.innerHTML = params.body || firmwareModalMarkup(params);
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-primary", text: t("c.close"), click: closeModal }));
				if (params.hasUpdate) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: t("md.updateNow"), click: () => { closeModal(); localStorage.setItem("update_notify", 0); espFlashGitWait(); } }));
			}
			break;
		case "espFlashGitWait":
			if (hText) hText.textContent = t("md.gitUpd");
			if (mBody) mBody.textContent = t("md.gitUpdBody");
			if (mBtns) mBtns.appendChild(el("div", { role: "status", "class": "spinner-border text-primary", append: el("span", { "class": classHide }) }));
			var waitTmr = setInterval(() => { fetch("/").then(() => { clearInterval(waitTmr); clearTimeout(timeoutTmr); closeModal(); console.log("[espFlashGitWait] hide modal"); window.location = "/"; }).catch(() => {}); }, 3000);
			var timeoutTmr = setTimeout(() => { clearInterval(waitTmr); if (mBtns) mBtns.innerHTML = ""; if (mBody) { mBody.textContent = t("md.noResp"); mBody.style.color = "red"; } if (mBtns) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: t("c.close"), click: closeModal })); }, 90000);
			break;
		case "espGitUpdate":
			if (hText) { hText.textContent = t("md.espUpdAvail"); hText.style.color = "#f7a623"; }
			if (mBody) mBody.textContent = params;
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-primary", text: t("md.later"), click: () => { closeModal(); localStorage.setItem("update_notify", 0); } }));
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-danger", text: t("md.noRemind"), click: () => { closeModal(); localStorage.setItem("update_notify", 1); } }));
			}
			break;
		case "zbFirmwareSelect":
			if (hText) hText.textContent = t("md.zbSel");
			if (mBody) mBody.innerHTML = "";
			if (mBtns) mBtns.innerHTML = "";
			if (mBody) mBody.appendChild(el("div", { "class": "container", style: "max-width: 400px", append: el("div", { "class": "row", id: "zbFirmwareSelectRow" }) }));
			const selRow = getEl("zbFirmwareSelectRow");
			if (selRow) {
				selRow.appendChild(el("button", { "class": "col-sm-12 mb-2 btn btn-primary", text: t("md.coord"), click: () => showZigbeeVersionInfo("coordinator") }));
				selRow.appendChild(el("button", { "class": "col-sm-12 mb-2 btn btn-outline-primary", text: t("md.router"), click: () => showZigbeeVersionInfo("router") }));
				selRow.appendChild(el("button", { type: "button", "class": "col-sm-12 btn btn-outline-secondary", text: t("c.cancel"), click: closeModal }));
			}
			break;
		case "zbVersionInfo":
			if (hText) hText.textContent = t("md.latestZb");
			if (mBody) mBody.innerHTML = params.body || firmwareModalMarkup(params);
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-primary", text: t("c.close"), click: closeModal }));
				if (params.canFlash) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: params.roleSwitch ? t("md.switchRole") : t("md.updateNow"), click: () => { closeModal(); requestZigbeeFlash(params.firmware, params.localRole, params.localVersion); } }));
			}
			break;
		case "zbRoleSwitchConfirm":
			if (hText) { hText.textContent = t("md.zbRoleSwitch"); hText.style.color = "#f7a623"; }
			if (mBody) mBody.textContent = t("md.zbRoleSwitchBody", { current: params.currentRole, target: params.targetRole });
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-outline-secondary", text: t("c.cancel"), click: closeModal }));
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-danger", text: t("md.switchRole"), click: () => { closeModal(); params.onConfirm(); } }));
			}
			break;
		case "rebootWait":
			if (hText) hText.textContent = t("md.devRestart");
			if (mBody) mBody.textContent = t("md.waitDev");
			if (mBtns) mBtns.appendChild(el("div", { role: "status", "class": "spinner-border text-primary", append: el("span", { "class": classHide }) }));
			var waitTmr = setInterval(() => { fetch("/").then(() => { clearInterval(waitTmr); clearTimeout(timeoutTmr); closeModal(); console.log("[rebootWait] hide modal"); window.location = "/"; }).catch(() => {}); }, 2000);
			var timeoutTmr = setTimeout(() => { clearInterval(waitTmr); if (mBtns) mBtns.innerHTML = ""; if (mBody) { mBody.textContent = t("md.noResp"); mBody.style.color = "red"; } if (mBtns) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: t("c.close"), click: closeModal })); }, 20000);
			break;
		case "saveOk":
			if (window.location.pathname == "/wifi") {
				if (hText) hText.textContent = t("md.wifiConn");
				if (mBody) mBody.textContent = t("md.wifiConnecting");
				if (mBtns) mBtns.appendChild(el("div", { role: "status", "class": "spinner-border text-primary", append: el("span", { "class": classHide }) }));
				let counter = 0;
				var getWifiIp = setInterval(() => {
					if (counter <= 15) {
						fetch(apiLink + api.actions.API_WIFICONNECTSTAT).then(r => r.json()).then(data => {
							if (data.connected) {
								espReboot();
								clearInterval(getWifiIp);
								setTimeout(() => {
									if (mBody) mBody.innerHTML = '<span style="color: green">' + t("d.connectedEx") + '</span><br>' + t("d.newIp") + data.ip + '<br>' + t("d.devReboot");
									if (mBtns) mBtns.innerHTML = "";
									if (mBtns) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-success", text: t("d.goTo") + data.ip, click: () => { window.location = "http://" + data.ip + "/"; } }));
								}, 5000);
							} else { counter++; }
						});
					} else {
						clearInterval(getWifiIp);
						if (mBody) { mBody.textContent = t("md.connErr"); mBody.style.color = "red"; }
						if (mBtns) mBtns.innerHTML = "";
						if (mBtns) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-success", text: t("c.close"), click: closeModal }));
					}
				}, 1000);
			} else {
				if (hText) hText.textContent = t("md.setSaved");
				if (mBtns) {
					mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: t("md.later2"), click: closeModal }));
					mBtns.appendChild(el("button", { type: "button", "class": "btn btn-success", text: t("md.restartNow"), click: () => { closeModal(); espReboot(); rebootWait(); } }));
				}
				if (mBody) mBody.textContent = t("md.savedBody");
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
	if (list) list.innerHTML = '<div class="wifi-item wifi-placeholder"><span class="wifi-spinner"></span><span class="wifi-name"><span class="busy-shimmer">' + t("w.scanning") + '</span></span></div>';
	let polls = 0;
	const giveUp = () => {
		if (!list) return;
		list.innerHTML = '<div class="wifi-item wifi-placeholder" onclick="getWifiList()"><span class="wifi-name">' + t("w.scanTimeout") + '</span></div>' + wifiJoinRowHTML();
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
// Built per call so the placeholder follows the active language.
function wifiPwInputHTML() {
	return '<input type="' + _wifiPwType + '" class="form-control form-control-sm' + (_wifiPwType === "text" ? " wifi-pw" : "") + '" placeholder="' + t("w.passPh") + '" autocomplete="' + (_wifiPwType === "text" ? "off" : "new-password") + '" readonly />';
}

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
		default: return t("d.unknown");
	}
}

const _wifiLockSvg = '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><rect x="3" y="7.5" width="10" height="6.5" rx="1.5"/><path d="M5.5 7.5V5.5a2.5 2.5 0 0 1 5 0v2"/></svg>';
const _wifiOpenSvg = '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"><circle cx="8" cy="8" r="6"/><path d="M8 7v3.5"/><circle cx="8" cy="5.2" r="0.9" fill="currentColor" stroke="none"/></svg>';
const _wifiChevSvg = '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" style="width:14px;height:14px"><path d="M6 3.5 10.5 8 6 12.5"/></svg>';
const _wifiCheckSvg = '<svg viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="width:15px;height:15px"><path d="M3 8.5 6.5 12 13 4.5"/></svg>';

// "Join Other Network…" row: always the last entry of the network list.
// Built per call so it follows the active language (see setLanguage).
function wifiJoinRowHTML() {
	return '<div class="wifi-item" id="wifiJoinOtherRow" onclick="wifiShowOther()">' +
		'<div class="wifi-row-main">' +
			'<span class="wifi-plus" aria-hidden="true">+</span>' +
			'<span class="wifi-name">' + t("w.joinOther") + '</span>' +
			'<span class="wifi-chev">' + _wifiChevSvg + '</span>' +
		'</div></div>';
}

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

function wifiJoinFailure(data) {
	const reason = Number(data && data.reason);
	// ESP-IDF disconnect reasons are numeric in the API; keep the UI independent
	// of the firmware headers while grouping them into useful advice. WiFi.status
	// is stale for the first seconds of a new begin(), so never classify from it.
	if (reason === 201) return t("w.noSsid");
	if (reason === 210 || reason === 211) return t("w.security");
	if (reason === 200 || reason === 212) return t("w.weakSignal");
	if ([2, 9, 14, 15, 18, 19, 20, 21, 22, 23, 24, 29, 202, 204].includes(reason)) return t("w.connFailPw");
	if (reason === 203) return t("w.apReject");
	if (reason === 205) return t("w.connFail");
	return null;
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
	if (statusEl) { statusEl.className = "wifi-join-status connecting"; statusEl.innerHTML = '<span class="busy-shimmer">' + t("w.connecting") + '</span>'; }
	wifiBusyBtn(btn, true);
	fetch("/saveParams", { method: "POST", body: fd }).then(r => {
		if (!r.ok) throw new Error("save failed");
		wifiPollConnect(seq, statusEl, ssid, pass, btn);
	}).catch(() => {
		if (seq !== _wifiJoinSeq) return;
		if (statusEl) { statusEl.className = "wifi-join-status err"; statusEl.textContent = t("w.saveFail"); }
		wifiBusyBtn(btn, false);
	});
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
				if (statusEl) { statusEl.className = "wifi-join-status ok"; statusEl.textContent = t("w.connected"); }
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
				const ipStatus = getEl("wifiIpStatus");
				if (ipStatus) { ipStatus.className = "wifi-ip-status"; ipStatus.textContent = ""; }
				renderWifiYourNetwork();
				renderWifiList();
			} else {
				const failure = wifiJoinFailure(data);
				if (!failure && tries < 15) return;
				clearInterval(tmr);
				wifiBusyBtn(btn, false);
				if (statusEl) { statusEl.className = "wifi-join-status err"; statusEl.textContent = failure || t("w.connFail"); }
			}
		}).catch(() => {
			if (seq !== _wifiJoinSeq) { clearInterval(tmr); return; }
			if (++tries >= 15) {
				clearInterval(tmr);
				wifiBusyBtn(btn, false);
				if (statusEl) { statusEl.className = "wifi-join-status err"; statusEl.textContent = t("w.connNoStatus"); }
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
		wifiTable.innerHTML = '<div class="wifi-item wifi-placeholder"><span class="wifi-name">' + t("w.noFound") + '</span></div>' + wifiJoinRowHTML();
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
			? '<span class="wifi-open" title="' + t("d.openNet") + '">' + _wifiOpenSvg + '</span>'
			: '<span class="wifi-lock" title="' + t("d.protected") + wifiSecurityLabel(n.secure) + '">' + _wifiLockSvg + '</span>';
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
				(open ? "" : wifiPwInputHTML()) +
				'<button type="button" class="btn btn-outline-primary btn-sm wifi-join">' + (open ? t("w.join") : t("w.connect")) + '</button>' +
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
				if (statusEl) { statusEl.className = "wifi-join-status err"; statusEl.textContent = t("w.pw8"); }
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

	wifiTable.insertAdjacentHTML("beforeend", wifiJoinRowHTML());
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
	if (statusEl) { statusEl.className = "wifi-ip-status"; statusEl.innerHTML = '<span class="busy-shimmer">' + t("d.saving") + '</span>'; }
	fetch("/saveParams", { method: "POST", body: fd }).then(() => {
		if (statusEl) { statusEl.className = "wifi-ip-status ok"; statusEl.textContent = t("w.saved"); }
	}).catch(() => {
		if (statusEl) { statusEl.className = "wifi-ip-status err"; statusEl.textContent = t("w.saveFail"); }
	});
}

// Forget the saved network: clear config, disconnect STA, update the UI.
function wifiForget() {
	if (!confirm(t("w.forgetConfirm"))) return;
	const ssidInput = getEl("WIFISSID");
	const pwdInput = getEl("WIFIpassword");
	if (ssidInput) ssidInput.value = "";
	if (pwdInput) pwdInput.value = "";
	_wifiSavedSecure = null;
	const fd = wifiFormData();
	const statusEl = getEl("wifiIpStatus");
	if (statusEl) { statusEl.className = "wifi-ip-status"; statusEl.innerHTML = '<span class="busy-shimmer">' + t("w.forgetting") + '</span>'; }
	fetch("/saveParams", { method: "POST", body: fd }).then(() => {
		if (statusEl) { statusEl.className = "wifi-ip-status ok"; statusEl.textContent = t("w.forgotten"); }
		toggleCollapseIp(false);
		renderWifiYourNetwork();
		renderWifiList();   // drop the checkmark from the list too
	}).catch(() => {
		if (statusEl) { statusEl.className = "wifi-ip-status err"; statusEl.textContent = t("w.forgetFail"); }
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
		if (statusEl) { statusEl.className = "wifi-join-status err"; statusEl.textContent = t("w.enterSsid"); }
		return;
	}
	if (!openNet && pass.length < 8) {
		const other = getEl("otherPass");
		if (other) other.classList.add("is-invalid");
		if (statusEl) { statusEl.className = "wifi-join-status err"; statusEl.textContent = t("w.pw8"); }
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

// Single path for filling backend values into data-replace cells, shared by
// page load, live status refresh and language switch - so pills/status text
// always render in the active locale. Only touches elements present in the DOM.
function fillReplaceValues(values) {
	for (const property in values) {
		if (property === "timeZoneName") continue; // dropdown handled by caller
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
								else elem.textContent = tStatus(valueToSet);
							}
					break;
			}
		});
	}
}

// The status page is identified by its Zigbee-link cell (present only there).
function isStatusPage() { return !!getQ("[data-replace='z2mConnected']"); }

// Live-refresh of the status page: poll /status (JSON of getRootData) and update
// the data-replace cells in place, without reloading the whole page.
function refreshStatusData() {
	if (!isStatusPage()) {
		if (statusRefreshTimer) { clearInterval(statusRefreshTimer); statusRefreshTimer = null; }
		return;
	}
	fetch("/status", { cache: "no-store" }).then(r => r.json()).then(values => {
		fillReplaceValues(values);
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
	return '<span class="status-pill ' + tone + '">' + tStatus(value) + '</span>';
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
		if (!combinedData || !combinedData.jsonData) return;
		var downloadCount = (combinedData.jsonData.assets || []).reduce((n, a) => n + (a.download_count || 0), 0);
		var releaseInfo = t("d.newFw", { tag: combinedData.jsonData.tag_name, count: downloadCount.toLocaleString() });
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
		modalConstructor("zbVersionInfo", { body: "<div class='alert alert-info'>" + t("d.loadingFw") + "</div><div class='text-muted'><div class='spinner-border text-primary' role='status'></div> <span class='busy-shimmer'>" + t("d.loading") + "</span></div>" });
		let fwData = null;
		let localVerData = null;
		let localRoleData = null;
		try {
			[fwData, localVerData, localRoleData] = await Promise.all([fetchZigbeeFirmware(), fetchData("/api?action=1&param=zbRev", false), fetchData("/api?action=1&param=zbRole", false)]);
		} catch (err) {
			console.warn("Failed to load:", err.message || err);
		}
		if (!fwData || (Array.isArray(fwData) && fwData.length === 0)) {
			const mBody = getQ(".modal-body");
			if (mBody) mBody.innerHTML = "<div class='alert alert-warning'><strong>" + t("d.noFwData") + "</strong><br>" + t("d.noFwDataSub") + "</div>";
			isZbVersionLoading = false;
			return;
		}
		let selectedFw = Array.isArray(fwData) ? fwData.find(fw => fw.role === firmwareType) : null;
		if (!selectedFw) {
			const mBody = getQ(".modal-body");
			if (mBody) mBody.innerHTML = "<div class='alert alert-warning'><strong>" + t("d.noFwFor") + t(firmwareType === "router" ? "md.router" : "md.coord") + "'</strong><br>" + t("d.availTypes") + "<br><ul>" + (Array.isArray(fwData) ? fwData.map(fw => "<li>" + escHtml(fw.name || "N/A") + " - " + escHtml(fw.firmwareType || "N/A") + "</li>").join("") : "<li>" + t("d.unknown") + "</li>") + "</ul></div>";
			isZbVersionLoading = false;
			return;
		}
		const localRole = normalizeZbRole(localRoleData);
		const localVer = localVerData && localVerData.trim().toLowerCase() !== "unknown" ? localVerData.trim() : "unknown";
		const remoteVer = selectedFw.version || "unknown";
		latestZbFirmware = { ...selectedFw, localRole, localVersion: localVer };
		// Compare raw backend tokens ("unknown" is always English on the wire);
		// only the display strings below are localized.
		const unknownRaw = localVer === "unknown", unknownRemote = remoteVer === "unknown";
		const roleSwitch = localRole !== "unknown" && localRole !== selectedFw.role;
		const hasUpdate = localRole === selectedFw.role && (unknownRaw || verIsNewer(remoteVer, localVer));
		const status = localRole === "unknown"
			? "<div class='alert alert-warning mt-2'><strong>" + t("d.roleUnknown") + "</strong></div>"
			: roleSwitch
				? "<div class='alert alert-warning mt-2'><strong>" + t("d.roleSwitch") + "</strong><br>" + t("d.installedRole") + " " + zbRoleLabel(localRole) + " -> " + zbRoleLabel(selectedFw.role) + "</div>"
				: !unknownRaw && !unknownRemote
					? (hasUpdate
						? "<div class='alert alert-success mt-2'><strong>" + t("d.updateAvailEx") + "</strong><br>" + t("d.currentLatest") + escHtml(localVer) + " -> " + t("d.latestIs") + escHtml(remoteVer) + "</div>"
						: "<div class='alert alert-secondary mt-2'><strong>" + t("d.upToDateEx") + "</strong><br>" + t("d.currentVer") + escHtml(localVer) + "</div>")
					: "";
		modalConstructor("zbVersionInfo", {
			summary: firmwareVersionSummary(localVer, remoteVer),
			details: [
				[t("d.name"), latestZbFirmware.name],
				[t("d.version"), latestZbFirmware.version],
				[t("d.installedRole"), zbRoleLabel(localRole)],
				[t("d.type"), latestZbFirmware.firmwareType],
				[t("d.file"), latestZbFirmware.asset],
				[t("d.desc"), latestZbFirmware.firmwareDesc]
			],
			changelog: latestZbFirmware.changelog || latestZbFirmware.releaseNotes || "",
			status,
			hasUpdate,
			canFlash: hasUpdate || roleSwitch,
			roleSwitch,
			firmware: selectedFw,
			localRole,
			localVersion: localVer
		});
	} catch (error) {
		console.error("Error while getting Zigbee version info:", error);
		const mBody = getQ(".modal-body");
		if (mBody) mBody.innerHTML = "<div class='alert alert-danger'><strong>" + t("d.errFetch") + "</strong><br>" + t("c.checkNet") + "<br><small>" + escHtml(error.message || error) + "</small></div>";
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

function firmwareVersionLabel(version) {
	const value = String(version == null ? "" : version).trim();
	if (!value || value.toLowerCase() === "unknown") return t("d.unknown");
	return /^v/i.test(value) ? escHtml(value) : "v" + escHtml(value);
}

function firmwareVersionSummary(local, remote) {
	return t("c.installed") + " " + firmwareVersionLabel(local) + "<br>" + t("d.latestIs") + " " + firmwareVersionLabel(remote);
}

function firmwareModalMarkup(params) {
	const details = (params.details || []).map(item => {
		const value = item[1] === undefined || item[1] === null || item[1] === "" ? t("d.unknown") : item[1];
		return "<li><strong>" + escHtml(item[0]) + "</strong> " + escHtml(value) + "</li>";
	}).join("");
	return "<div class='alert alert-info'>" + params.summary + "</div>" +
		"<p><strong>" + t("d.fwDetails") + "</strong></p><ul>" + details + "</ul>" +
		"<p><strong>" + t("d.releaseNotes") + "</strong></p>" +
		"<pre class='fw-modal-changelog'>" + escHtml(params.changelog || t("d.noNotes")) + "</pre>" +
		(params.status || "");
}

let espFwInfo = null;
let _espFwError = false;

// Render the ESP32 card from cache (no network) - shared by the version check
// and language switch, so the status text follows the active locale.
function renderEspFwStatus() {
	const st = getEl("espFwStatus");
	if (!st) return;
	const upd = getEl("updEspGit");
	const log = getEl("espFwChglog");
	if (_espFwError || !espFwInfo) {
		if (!_espFwError) return; // check still running: leave the spinner text
		st.innerHTML = "<span class='text-muted'>" + t("d.noGh") + "</span>";
		if (upd) upd.style.display = "none";
		if (log) log.style.display = "none";
		return;
	}
	if (espFwInfo.hasUpdate) {
		st.innerHTML = t("d.updAvail") + "<strong>" + escHtml(espFwInfo.tag) + "</strong> (" + t("d.dlCount", { count: (espFwInfo.count || 0).toLocaleString() }) + ")";
		if (log) { log.style.display = ""; log.textContent = espFwInfo.body || ""; }
		if (upd) upd.style.display = "";
	} else {
		st.innerHTML = t("d.upToDate") + " (<strong>" + escHtml(espFwInfo.local) + "</strong>)";
		if (upd) upd.style.display = "none";
		if (log) log.style.display = "none";
	}
}

// Details modal for the ESP32 card: installed + latest + release notes,
// available for any version (not just when an update is pending).
function showEspFwDetails() {
	if (!espFwInfo) { checkEspFirmware(); return; }
	const local = espFwInfo.local || "unknown";
	const remote = espFwInfo.tag || "unknown";
	const status = espFwInfo.hasUpdate
		? "<div class='alert alert-success mt-2'><strong>" + t("d.updateAvailEx") + "</strong><br>" + t("d.currentLatest") + escHtml(local) + " -> " + t("d.latestIs") + escHtml(remote) + "</div>"
		: "<div class='alert alert-secondary mt-2'><strong>" + t("d.upToDateEx") + "</strong><br>" + t("d.currentVer") + escHtml(local) + "</div>";
	modalConstructor("espFwDetails", {
		summary: firmwareVersionSummary(local, remote),
		details: [
			[t("d.name"), espFwInfo.name || remote],
			[t("d.version"), remote],
			[t("d.type"), "ESP32"],
			[t("d.file"), espFwInfo.asset || "AVATTO-GW90-Ti.bin"]
		],
		changelog: espFwInfo.body,
		status,
		hasUpdate: espFwInfo.hasUpdate
	});
}

// ESP32 firmware status on the About page: only offers an update (and shows
// the changelog) when GitHub actually has something newer.
async function checkEspFirmware() {
	const st = getEl("espFwStatus");
	if (!st) return;
	st.innerHTML = "<span class='text-muted busy-shimmer'>" + t("c.checking") + "</span>";
	let combined = null;
	try {
		combined = await processResponses();
	} catch (e) { console.warn("ESP32 version check failed:", e); }
	espFwInfo = null;
	_espFwError = false;
	if (!combined || !combined.jsonData || !combined.jsonData.tag_name) {
		_espFwError = true;
		renderEspFwStatus();
		return;
	}
	const tag = combined.jsonData.tag_name;
	const local = String(combined.textData).trim();
	const count = (combined.jsonData.assets || []).reduce((n, a) => n + (a.download_count || 0), 0);
	const asset = (combined.jsonData.assets || []).find(a => a.name && a.name.endsWith(".bin"));
	espFwInfo = { tag, local, name: combined.jsonData.name || tag, asset: asset && asset.name, body: combined.jsonData.body || "", hasUpdate: verIsNewer(tag, combined.textData), count };
	renderEspFwStatus();
}

function zbFirmwareFromReleases(releases) {
	const includePrerelease = localStorage.getItem(zbFwIncludePrereleaseKey) === "1";
	return ["coordinator", "router"].map(role => {
		const candidates = (Array.isArray(releases) ? releases : []).filter(release => {
			const tag = String(release && release.tag_name || "").toLowerCase();
			return release && !release.draft && (includePrerelease || !release.prerelease) && tag.includes("_" + role + "_");
		}).sort((a, b) => String(b.published_at || b.created_at).localeCompare(String(a.published_at || a.created_at)));
		const release = candidates.find(candidate => {
			return (candidate.assets || []).some(asset => {
				const name = String(asset.name || "").toLowerCase();
				return name.startsWith("cc1352p2_cc2652p_launchpad_" + role + "_") && name.endsWith(".zip");
			});
		});
		if (!release) return null;
		const asset = (release.assets || []).find(candidate => {
			const name = String(candidate.name || "").toLowerCase();
			return name.startsWith("cc1352p2_cc2652p_launchpad_" + role + "_") && name.endsWith(".zip");
		});
		const versionMatch = String(release.tag_name || "").match(/\d{8}/);
		return {
			name: release.name || release.tag_name,
			version: versionMatch ? versionMatch[0] : release.tag_name,
			firmwareType: "zigbee",
			firmwareDesc: "Koenkk Z-Stack firmware for CC2652P",
			role,
			asset: asset.name,
			link: asset.browser_download_url,
			changelog: release.body || "",
			prerelease: !!release.prerelease,
		};
	}).filter(Boolean);
}

async function fetchZigbeeFirmware() {
	return zbFirmwareFromReleases(await fetchData(zbFwReleasesUrl, true));
}

function initZbFirmwareSource() {
	const prerelease = getEl("zbIncludePrerelease");
	if (!prerelease) return;
	prerelease.checked = localStorage.getItem(zbFwIncludePrereleaseKey) === "1";
	prerelease.addEventListener("change", () => {
		localStorage.setItem(zbFwIncludePrereleaseKey, prerelease.checked ? "1" : "0");
		_zbFwCache = null;
		checkZbFirmware();
	});
}

// Zigbee firmware status on the About page: per-type rows that
// only offer a flash when the selected Koenkk release is newer.
async function checkZbFirmware() {
	const st = getEl("zbFwStatus");
	if (!st) return;
	st.innerHTML = "<span class='text-muted busy-shimmer'>" + t("c.checking") + "</span>";
	let fwData = null, rev = "", role = "";
	try {
		[fwData, rev, role] = await Promise.all([fetchZigbeeFirmware(), fetchData("/api?action=1&param=zbRev", false), fetchData("/api?action=1&param=zbRole", false)]);
	} catch (e) { console.warn("Zigbee version check failed:", e); }
	const localRev = (rev || "").trim();
	const localRole = normalizeZbRole(role);
	fillReplaceValues({
		zigbeeFwRev: localRev || "unknown",
		zigbeeFwRole: localRole === "router" ? "Router" : localRole === "coordinator" ? "Coordinator" : "Unknown"
	});
	if (!Array.isArray(fwData) || !fwData.length) {
		_zbFwCache = { error: "index" };
	} else {
		_zbFwCache = fwData.length ? { fws: fwData, localRev, localRole } : { error: "empty" };
	}
	renderZbFwList();
}

let _zbFwCache = null; // {fws, localRev} | {error:'index'|'empty'}

// Render the Zigbee firmware rows from cache (no network) - shared by the
// version check and language switch, so rows follow the active locale.
function renderZbFirmwareRow(fw, localRole, localRev, allowRoleSwitch) {
	const roleMatches = fw.role === localRole;
	const remote = fw.version || "unknown";
	const hasUpdate = roleMatches && verIsNewer(remote, localRev);
	const row = el("div", { "class": "zb-fw-row" });
	const info = el("div", { "class": "zb-fw-info" });
	const roleLabel = fw.role === "router" ? t("d.router") : t("d.coord");
	const status = roleMatches ? (hasUpdate ? t("d.updAvailShort") : t("d.upToDate")) : t("d.roleSwitch");
	info.innerHTML = "<strong>" + roleLabel + "</strong>" +
		(fw.role === "coordinator" ? " <span class='default-badge'>" + t("d.default") + "</span>" : "") +
		(fw.prerelease ? " <span class='default-badge'>" + t("d.prerelease") + "</span>" : "") +
		"<div class='text-muted zb-fw-sub'>" + t("d.latestIs") + escHtml(remote) + " · " +
		((hasUpdate || !roleMatches) ? "<span style='color:#f7a623'>" + status + "</span>" : status) + "</div>";
	row.appendChild(info);
	const wrap = el("span", { "class": "zb-fw-btns" });
	wrap.appendChild(el("button", { type: "button", "class": "btn btn-outline-primary btn-sm", text: t("c.details"), click: () => showZigbeeVersionInfo(fw.role) }));
	if (hasUpdate || (allowRoleSwitch && !roleMatches)) {
		wrap.appendChild(document.createTextNode(" "));
		wrap.appendChild(el("button", {
			type: "button", "class": "btn btn-warning btn-sm", text: hasUpdate ? t("d.flash") + remote : t("d.switchTo") + roleLabel, click: () => requestZigbeeFlash(fw, localRole, localRev || "unknown")
		}));
	}
	row.appendChild(wrap);
	return row;
}

// Keep the installed role prominent; alternate-role flashing is deliberately secondary.
function renderZbFwList() {
	const st = getEl("zbFwStatus");
	const box = getEl("zbFwList");
	const roleSwitch = getEl("zbFwRoleSwitch");
	const alternateBox = getEl("zbFwAlternateList");
	if (!st || !_zbFwCache) return; // check still running: leave the spinner text
	if (_zbFwCache.error) {
		st.innerHTML = "<span class='text-muted'>" + t(_zbFwCache.error === "empty" ? "d.noZbFw" : "d.noZbIdx") + "</span>";
		if (box) box.innerHTML = "";
		if (roleSwitch) roleSwitch.style.display = "none";
		return;
	}
	const { fws, localRev, localRole } = _zbFwCache;
	if (box) box.innerHTML = "";
	if (alternateBox) alternateBox.innerHTML = "";
	if (localRole === "unknown") {
		st.innerHTML = "<span style='color:#f7a623'><strong>" + t("d.roleUnknown") + "</strong></span>";
		if (roleSwitch) roleSwitch.style.display = "none";
		return;
	}

	const current = fws.find(fw => fw.role === localRole);
	const other = fws.find(fw => fw.role !== localRole);
	if (current && box) {
		box.appendChild(renderZbFirmwareRow(current, localRole, localRev, false));
	}
	if (other && alternateBox) {
		alternateBox.appendChild(renderZbFirmwareRow(other, localRole, localRev, true));
	}
	if (roleSwitch) roleSwitch.style.display = other ? "" : "none";

	if (!current) {
		st.innerHTML = "<span style='color:#f7a623'><strong>" + t("d.noCurrentRoleFw") + "</strong></span>";
		return;
	}
	const currentUpdate = verIsNewer(current.version || "unknown", localRev);
	st.innerHTML = currentUpdate
		? "<span style='color:#f7a623'><strong>" + t("d.fwUpdateAvailable", { role: zbRoleLabel(localRole) }) + "</strong></span>"
		: t("d.fwUpToDate", { role: zbRoleLabel(localRole) });
}


// ========== Lightweight i18n (MicroI18n) ==========
// Culturally adapted, not word-for-word: DE uses WLAN (not WiFi),
// neutral infinitives and Sie-free short labels; ZH uses Simplified,
// iOS-style terms (忽略此网络, 热点) familiar to Chinese users.
const I18N = {
en: {
"l.st": "Status", "l.ge": "General", "l.ne": "Ethernet", "l.wf": "WiFi", "l.zi": "Z2M/ZHA", "l.se": "Security", "l.mq": "MQTT", "l.vp": "WireGuard", "l.to": "Tools", "l.ab": "About", "l.lo": "Logout",
"c.save": "Save", "c.cancel": "Cancel", "c.close": "Close", "c.restart": "Restart", "c.details": "Details", "c.checkUpd": "Check for updates", "c.checking": "Checking for updates...", "c.selFile": "Select File...", "c.uploading": "Uploading firmware...", "c.uplFail": "Firmware upload failed!", "c.checkNet": "Please check network connection and retry", "c.enable": "Enable", "c.installed": "Installed:",
"s.opMode": "Operating mode", "s.zbConn": "Zigbee connection", "s.conn": "Connection:", "s.clientIp": "Client IP:", "s.connFor": "Connected for:", "s.coordStatus": "Coordinator status:", "s.coordResets": "Coordinator auto-resets:", "s.eth": "Ethernet", "s.ethStatus": "Connection status:", "s.dhcp": "DHCP:", "s.ip": "IP Address:", "s.mask": "Mask:", "s.gw": "Gateway:", "s.speed": "Connection Speed:", "s.mac": "MAC:", "s.devStatus": "Device Status", "s.opModeRow": "Operating mode:", "s.ethernet": "Ethernet:", "s.uplink": "Network uplink:", "s.uptime": "Uptime:", "s.temp": "Temperature:", "s.wifiEn": "WiFi Enabled:", "s.wifiStatus": "WiFi Status:", "s.wifiApEn": "WiFi Access Point Enabled:", "s.wifiApStatus": "WiFi Access Point Status:", "s.wifi": "WiFi", "s.mode": "Mode:", "s.ssid": "SSID:", "s.subnet": "Subnet Mask:", "s.signal": "Signal:", "s.vpn": "VPN", "s.init": "Initialized:", "s.remote": "Remote Address:",
"g.workMode": "Work Mode", "g.netMode": "Network Mode", "g.recommended": "Recommended", "g.netDesc": "Ethernet with automatic Wi-Fi fallback", "g.serMode": "Serial Mode", "g.serDesc": "Directly connect to computer via USB (serial test)", "g.keepWeb": "Keep Network & Web", "g.keepHint": "Keeps one channel active (Wi-Fi, Ethernet or Wi-Fi AP, in that order) so the web interface stays reachable.", "g.rtControl": "Real-time Control", "g.ledHint": "Use toggle buttons to control LED status (current session, not saved after reboot)", "g.pwrLed": "Power Indicator LED", "g.wifiLed": "Wi-Fi/Serial Mode LED", "g.ledSaveHint": "Use the switches below to control LED behavior (saved after reboot)", "g.disPwr": "Disable Power LED", "g.disUsb": "Disable USB LED", "g.sysVar": "System Variables", "g.hostname": "Hostname (The device will be displayed on the network with this name)", "g.logRefresh": "Log refresh interval (milliseconds)", "g.timezone": "Time Zone",
"e.conn": "Connection", "e.dhcp": "Enable DHCP (recommended)", "e.ipSet": "IP Settings", "e.ip": "IP", "e.mask": "Subnet Mask", "e.gw": "Default Gateway",
"w.title": "Wi-Fi", "w.yourNet": "Your Network", "w.ipSet": "IP Settings", "w.dhcp": "DHCP", "w.asleep": "WiFi is asleep (Ethernet is the active uplink) - live IP appears here when WiFi is used.", "w.ip": "IP Address", "w.mask": "Subnet Mask", "w.router": "Router", "w.save": "Save", "w.forget": "Forget This Network", "w.noNet": "No network configured", "w.otherNet": "Other Networks", "w.joinOther": "Join Other Network…", "w.ssidPh": "Network name (SSID)", "w.passPh": "Password", "w.join": "Join", "w.connect": "Connect", "w.scanning": "Scanning…", "w.scanTimeout": "Scan timed out. Tap to retry.", "w.noFound": "No Wi-Fi networks found.", "w.connecting": "Connecting…", "w.connected": "Connected", "w.connFailPw": "Wi-Fi authentication failed. Check the password and security mode.", "w.noSsid": "Network not found. Check the SSID, range, and that the network is broadcasting.", "w.security": "This network's security mode is not supported. Check the router settings.", "w.weakSignal": "The Wi-Fi signal is too weak or unstable. Move closer to the router and try again.", "w.apReject": "The access point rejected the connection. Check its settings and try again.", "w.connFail": "Could not connect. Check the SSID, password, and router.", "w.connNoStatus": "Connection status unavailable.", "w.saveFail": "Save failed. Check your network.", "w.saved": "Saved", "w.forgetting": "Forgetting…", "w.forgotten": "Network forgotten.", "w.forgetFail": "Failed to forget network.", "w.forgetConfirm": "Forget this Wi-Fi network?", "w.enterSsid": "Enter a network name.", "w.pw8": "Password must be at least 8 characters.", "w.wpa": "WPA2/WPA3", "w.open": "Open", "w.noNetsYet": "No networks yet.",
"z.serSet": "Serial Settings", "z.baud": "Baud Rate", "z.port": "Port", "z.cfgGen": "Config Generator", "z.apAlert": "It seems that AVATTO-GW90-Ti is operating in access point mode (IP address is 192.168.1.1). The IP address will change after connecting to Ethernet or Wi-Fi, and the configuration needs to be regenerated.", "z.cfgGenLabel": "Config Generator", "z.copy": "Copy to Clipboard",
"sec.webSrv": "Web Server", "sec.disWeb": "Disable Web Server when interface is connected", "sec.enAuth": "Enable Web Server Authentication", "sec.user": "Username", "sec.pass": "Password", "sec.access": "Access Control", "sec.enWhite": "Enable Connection IP Whitelist", "sec.allowed": "Allowed IPs",
"m.conn": "Connection", "m.server": "Server", "m.port": "Port", "m.user": "Username", "m.pass": "Password", "m.topicDisc": "Topic & Discovery", "m.topic": "Topic", "m.interval": "Interval (seconds)", "m.haDisc": "Enable Home Assistant Discovery",
"wg.tunnel": "Tunnel", "wg.localAddr": "Local Address", "wg.localAddrEx": "e.g. 10.0.0.2/24", "wg.privKey": "Local Private Key", "wg.privKeyPh": "Enter WireGuard private key", "wg.privNote": "44-character base64 private key", "wg.endpoint": "Endpoint", "wg.remoteAddr": "Remote Endpoint Address", "wg.remoteNote": "IP address or domain of the remote endpoint", "wg.remotePub": "Remote Public Key", "wg.remotePubNote": "44-character base64 public key", "wg.port": "Port", "wg.portNote": "UDP port, 1-65535",
"t.logs": "Logs", "t.esp": "ESP32", "t.zbFrames": "Zigbee ZNP Frames", "t.human": "Human-readable frames", "t.clear": "Clear Console", "t.checkConn": "Check Zigbee Connection", "t.checkVer": "Check Zigbee Version", "t.modCtrl": "Module Control", "t.rstZb": "Restart Zigbee", "t.zbRefresh": "Zigbee Refresh Mode", "t.routerRe": "Router Reconnect", "t.rstEsp": "Restart ESP32",
"a.device": "Device", "a.brand": "Brand:", "a.name": "Name:", "a.nameVal": "AVATTO Zigbee Smart Gateway", "a.fw": "Firmware:", "a.date": "Date:", "a.sysInfo": "System Information", "a.zstar": "Zig-Star Model:", "a.mcu": "MCU:", "a.zb": "Zigbee:", "a.eth": "Ethernet:", "a.ifaces": "Interfaces:", "a.if1": "RJ45 Ethernet (Supports PoE)", "a.if2": "USB Type-C", "a.if3": "Zigbee Antenna Interface", "a.espFwVer": "ESP32 Firmware Version:", "a.espFreq": "ESP32 Frequency:", "a.cores": "Cores @", "a.flashSize": "ESP32 Flash Size:", "a.mem": "ESP32 Memory (Free/Total):", "a.zbHw": "CC2652P Hardware Version:", "a.zbFw": "CC2652P Firmware Version:", "a.espFw": "ESP32 Firmware", "a.installGh": "Install update from GitHub", "a.coordNote": "Coordinator is the default choice for ZHA and Zigbee2MQTT. Flash Router only to join or extend another Zigbee network.", "a.zbFwCard": "Zigbee Firmware", "a.rev": "rev", "a.espFwUpd": "ESP32 file update", "a.zbFwUpd": "Zigbee File Update",
"md.warn": "WARNING", "md.warnBody": "Flashing unofficial, incorrect or corrupted firmware can damage or brick your device!!!", "md.agree": "I agree and I am taking the risk", "md.rstEsp": "Restart ESP32", "md.rstBody": "The device will restart and the connection will be interrupted for a few seconds. Continue?", "md.espFw": "ESP32 firmware", "md.updateNow": "Update now", "md.gitUpd": "ESP32 GIT UPDATE", "md.gitUpdBody": "Downloading and flashing latest release from Github. This window will automatically close when the device reboots.", "md.later": "Later", "md.noRemind": "Don't remind", "md.zbSel": "Select Zigbee Firmware Type", "md.coord": "Coordinator", "md.router": "Router", "md.latestZb": "Latest Zigbee Firmware", "md.devRestart": "DEVICE RESTART", "md.waitDev": "Waiting for device... This window will automatically close when the device reboots.", "md.wifiConn": "Wi-Fi network connection", "md.wifiConnecting": "Connecting to the network in progress... Wait for the result.", "md.connErr": "Connection error, check SSID, PASSWORD and try again", "md.setSaved": "Settings saved", "md.savedBody": "New parameters saved. Some settings require a reboot.", "md.later2": "Restart manually later", "md.restartNow": "Restart now", "md.noResp": "No response from the device, this may happen if the device changed IP address or if the USB mode was selected.",
"d.saveErr": "Error saving settings. Check your network", "d.netErr": "Error. Check your network", "d.pageErr": "Page load error: ", "d.githubStart": "Github download starting...", "d.prepDev": "Preparing device...", "d.invalidLink": "Invalid firmware link", "d.dlFw": "Downloading firmware...", "d.dlFail": "Download failed: ", "d.evTimeout": "EventSource timeout", "d.connFailPoll": "Connection failed, using polling mode...", "d.downloaded": "Downloading... ", "d.dlComplete": "Download complete, starting flash...", "d.flashing": "Flashing... ", "d.flashDone": "Flash complete!", "d.validating": "Validating and flashing...", "d.progress": "progress: ", "d.updDoneReboot": "Update completed!<br>Rebooting!", "d.updAvail": "Update available: ", "d.upToDate": "Up to date", "d.noGh": "Could not reach GitHub. Use file upload below.", "d.noZbIdx": "Could not reach the firmware index. Use file upload below.", "d.noZbFw": "No Zigbee firmware in the index.", "d.updAvailShort": "update available", "d.allUpToDate": "All variants up to date", "d.noFwData": "No firmware data available.", "d.noFwDataSub": "Please check your internet connection or try again later.", "d.noFwFor": "No firmware found for '", "d.availTypes": "Available firmware types:", "d.fwDetails": "Firmware Details:", "d.name": "Name:", "d.version": "Version:", "d.type": "Type:", "d.desc": "Description:", "d.baud": "Baud Rate:", "d.dongle": "Dongle Type:", "d.localVer": "Local version: v", "d.ghVer": "GitHub version: ", "d.file": "File: ", "d.errFetch": "Error fetching version information.", "d.updateAvailEx": "Update Available!", "d.currentLatest": "Current: v", "d.upToDateEx": "Up to date", "d.currentVer": "Current version: v", "d.router": "Router", "d.coord": "Coordinator", "d.default": "default", "d.flash": "Flash ", "d.protected": "Protected · ", "d.unknown": "UNKNOWN", "d.uploadPct": "Uploading: ", "d.uplComplete": "Upload complete! <br>Start verifing & flashing...", "d.opTimeout": "Operation timed out. Please check device status.", "d.showHidePw": "Show / hide password", "d.showHideKey": "Show / hide private key", "d.darkMode": "Dark Mode", "d.lightMode": "Light Mode", "d.ipSettings": "IP settings", "d.loadingFw": "Loading firmware information...", "d.loading": "Loading...", "d.goTo": "Go to ", "d.newIp": "New IP address is ", "d.devReboot": "Device will now reboot for the new settings to take effect.", "d.connectedEx": "Connected!", "d.logFilter": "Log filter", "md.espUpdAvail": "ESP32 update available", "d.newFw": "New firmware (${tag}) was found on GitHub. It was downloaded ${count} times. Go -> About -> ESP32 Firmware to update.", "d.latestIs": "Latest: ", "d.dlCount": "downloaded ${count} times", "d.updatesAvail": "${n} update(s) available", "d.localRev": "local rev", "d.noNotes": "(no release notes)", "d.saving": "Saving…", "d.openNet": "Open network", "d.language": "Language", "d.toggleDark": "Toggle dark mode",
"sv.yes": "Yes", "sv.no": "No", "sv.on": "On", "sv.off": "Off", "sv.connected": "Connected", "sv.notConn": "Not connected", "sv.active": "Active", "sv.asleep": "Asleep", "sv.notCfg": "Not configured", "sv.client": "Client", "sv.ap": "AP", "sv.accessPoint": "Access Point", "sv.ethernet": "Ethernet", "sv.wifi": "WiFi", "sv.wifiFb": "WiFi (fallback)", "sv.none": "None", "sv.apStarted": "AP started", "sv.notStarted": "Not started", "sv.serialPass": "Serial passthrough", "sv.usbPass": "USB passthrough", "sv.zbNet": "Zigbee-to-Network", "sv.zbSer": "Zigbee-to-Serial", "sv.unknown": "unknown", "sv.embedded": "embedded", "sv.external": "external"
},
de: {
"l.st": "Status", "l.ge": "Allgemein", "l.ne": "Ethernet", "l.wf": "WLAN", "l.zi": "Z2M/ZHA", "l.se": "Sicherheit", "l.mq": "MQTT", "l.vp": "WireGuard", "l.to": "Werkzeuge", "l.ab": "Über", "l.lo": "Abmelden",
"c.save": "Speichern", "c.cancel": "Abbrechen", "c.close": "Schließen", "c.restart": "Neustart", "c.details": "Details", "c.checkUpd": "Nach Updates suchen", "c.checking": "Suche nach Updates …", "c.selFile": "Datei wählen …", "c.uploading": "Firmware wird hochgeladen …", "c.uplFail": "Firmware-Upload fehlgeschlagen!", "c.checkNet": "Bitte Netzwerkverbindung prüfen und erneut versuchen", "c.enable": "Aktivieren", "c.installed": "Installiert:",
"s.opMode": "Betriebsmodus", "s.zbConn": "Zigbee-Verbindung", "s.conn": "Verbindung:", "s.clientIp": "Client-IP:", "s.connFor": "Verbunden seit:", "s.coordStatus": "Koordinatorstatus:", "s.coordResets": "Auto-Neustarts des Koordinators:", "s.eth": "Ethernet", "s.ethStatus": "Verbindungsstatus:", "s.dhcp": "DHCP:", "s.ip": "IP-Adresse:", "s.mask": "Netzmaske:", "s.gw": "Gateway:", "s.speed": "Verbindungsgeschwindigkeit:", "s.mac": "MAC:", "s.devStatus": "Gerätestatus", "s.opModeRow": "Betriebsmodus:", "s.ethernet": "Ethernet:", "s.uplink": "Netzwerk-Uplink:", "s.uptime": "Betriebszeit:", "s.temp": "Temperatur:", "s.wifiEn": "WLAN aktiviert:", "s.wifiStatus": "WLAN-Status:", "s.wifiApEn": "WLAN-Zugangspunkt aktiviert:", "s.wifiApStatus": "Status des WLAN-Zugangspunkts:", "s.wifi": "WLAN", "s.mode": "Modus:", "s.ssid": "SSID:", "s.subnet": "Netzmaske:", "s.signal": "Signal:", "s.vpn": "VPN", "s.init": "Initialisiert:", "s.remote": "Remote-Adresse:",
"g.workMode": "Betriebsart", "g.netMode": "Netzwerkmodus", "g.recommended": "Empfohlen", "g.netDesc": "Ethernet mit automatischem WLAN-Fallback", "g.serMode": "Serieller Modus", "g.serDesc": "Direkt per USB mit dem Computer verbinden (serieller Test)", "g.keepWeb": "Netzwerk & Web behalten", "g.keepHint": "Hält einen Kanal aktiv (WLAN, Ethernet oder WLAN-AP, in dieser Reihenfolge), damit die Weboberfläche erreichbar bleibt.", "g.rtControl": "Echtzeitsteuerung", "g.ledHint": "LEDs per Tastendruck schalten (nur aktuelle Sitzung, geht nach Neustart verloren)", "g.pwrLed": "Power-LED", "g.wifiLed": "WLAN/Seriell-LED", "g.ledSaveHint": "Schalter unten steuern das LED-Verhalten dauerhaft (bleibt nach Neustart)", "g.disPwr": "Power-LED deaktivieren", "g.disUsb": "USB-LED deaktivieren", "g.sysVar": "Systemvariablen", "g.hostname": "Hostname (unter diesem Namen erscheint das Gerät im Netzwerk)", "g.logRefresh": "Log-Aktualisierung (Millisekunden)", "g.timezone": "Zeitzone",
"e.conn": "Verbindung", "e.dhcp": "DHCP aktivieren (empfohlen)", "e.ipSet": "IP-Einstellungen", "e.ip": "IP", "e.mask": "Netzmaske", "e.gw": "Standard-Gateway",
"w.title": "WLAN", "w.yourNet": "Eigenes Netzwerk", "w.ipSet": "IP-Einstellungen", "w.dhcp": "DHCP", "w.asleep": "WLAN ist im Standby (Ethernet ist der aktive Uplink) – Live-IP erscheint hier, sobald WLAN genutzt wird.", "w.ip": "IP-Adresse", "w.mask": "Netzmaske", "w.router": "Router", "w.save": "Speichern", "w.forget": "Dieses Netzwerk entfernen", "w.noNet": "Kein Netzwerk eingerichtet", "w.otherNet": "Weitere Netzwerke", "w.joinOther": "Anderes Netzwerk hinzufügen …", "w.ssidPh": "Netzwerkname (SSID)", "w.passPh": "Passwort", "w.join": "Beitreten", "w.connect": "Verbinden", "w.scanning": "Suche läuft …", "w.scanTimeout": "Suche abgelaufen. Zum Wiederholen tippen.", "w.noFound": "Keine WLAN-Netzwerke gefunden.", "w.connecting": "Verbinde …", "w.connected": "Verbunden", "w.connFailPw": "WLAN-Authentifizierung fehlgeschlagen. Passwort und Sicherheitsmodus prüfen.", "w.noSsid": "Netzwerk nicht gefunden. SSID, Reichweite und sichtbare Ausstrahlung prüfen.", "w.security": "Der Sicherheitsmodus dieses Netzwerks wird nicht unterstützt. Router-Einstellungen prüfen.", "w.weakSignal": "WLAN-Signal zu schwach oder instabil. Näher an den Router gehen und erneut versuchen.", "w.apReject": "Der Access Point hat die Verbindung abgelehnt. Einstellungen prüfen und erneut versuchen.", "w.connFail": "Verbindung nicht möglich. SSID, Passwort und Router prüfen.", "w.connNoStatus": "Verbindungsstatus nicht verfügbar.", "w.saveFail": "Speichern fehlgeschlagen. Netzwerk prüfen.", "w.saved": "Gespeichert", "w.forgetting": "Wird entfernt …", "w.forgotten": "Netzwerk entfernt.", "w.forgetFail": "Entfernen fehlgeschlagen.", "w.forgetConfirm": "Dieses WLAN-Netzwerk entfernen?", "w.enterSsid": "Netzwerknamen eingeben.", "w.pw8": "Das Passwort braucht mindestens 8 Zeichen.", "w.wpa": "WPA2/WPA3", "w.open": "Offen", "w.noNetsYet": "Noch keine Netzwerke.",
"z.serSet": "Serielle Einstellungen", "z.baud": "Baudrate", "z.port": "Port", "z.cfgGen": "Konfigurationsgenerator", "z.apAlert": "Das Gerät läuft wohl im Access-Point-Modus (IP 192.168.1.1). Nach Verbindung per Ethernet oder WLAN ändert sich die IP – bitte Konfiguration neu erzeugen.", "z.cfgGenLabel": "Konfigurationsgenerator", "z.copy": "In Zwischenablage kopieren",
"sec.webSrv": "Webserver", "sec.disWeb": "Webserver bei aktiver Verbindung deaktivieren", "sec.enAuth": "Webserver-Authentifizierung aktivieren", "sec.user": "Benutzername", "sec.pass": "Passwort", "sec.access": "Zugriffskontrolle", "sec.enWhite": "IP-Whitelist für Verbindungen aktivieren", "sec.allowed": "Erlaubte IPs",
"m.conn": "Verbindung", "m.server": "Server", "m.port": "Port", "m.user": "Benutzername", "m.pass": "Passwort", "m.topicDisc": "Topic & Discovery", "m.topic": "Topic", "m.interval": "Intervall (Sekunden)", "m.haDisc": "Home-Assistant-Discovery aktivieren",
"wg.tunnel": "Tunnel", "wg.localAddr": "Lokale Adresse", "wg.localAddrEx": "z. B. 10.0.0.2/24", "wg.privKey": "Lokaler privater Schlüssel", "wg.privKeyPh": "WireGuard-Private-Key eingeben", "wg.privNote": "44-stelliger Base64-Private-Key", "wg.endpoint": "Endpunkt", "wg.remoteAddr": "Remote-Endpunkt-Adresse", "wg.remoteNote": "IP-Adresse oder Domain des Remote-Endpunkts", "wg.remotePub": "Öffentlicher Schlüssel (Remote)", "wg.remotePubNote": "44-stelliger Base64-Public-Key", "wg.port": "Port", "wg.portNote": "UDP-Port, 1–65535",
"t.logs": "Protokolle", "t.esp": "ESP32", "t.zbFrames": "Zigbee-ZNP-Frames", "t.human": "Lesbare Frame-Ansicht", "t.clear": "Konsole leeren", "t.checkConn": "Zigbee-Verbindung prüfen", "t.checkVer": "Zigbee-Version prüfen", "t.modCtrl": "Modulsteuerung", "t.rstZb": "Zigbee neu starten", "t.zbRefresh": "Zigbee-Auffrischmodus", "t.routerRe": "Router erneut verbinden", "t.rstEsp": "ESP32 neu starten",
"a.device": "Gerät", "a.brand": "Marke:", "a.name": "Name:", "a.nameVal": "AVATTO Zigbee Smart Gateway", "a.fw": "Firmware:", "a.date": "Datum:", "a.sysInfo": "Systeminformationen", "a.zstar": "Zig-Star-Modell:", "a.mcu": "MCU:", "a.zb": "Zigbee:", "a.eth": "Ethernet:", "a.ifaces": "Schnittstellen:", "a.if1": "RJ45-Ethernet (mit PoE)", "a.if2": "USB Typ-C", "a.if3": "Zigbee-Antennenanschluss", "a.espFwVer": "ESP32-Firmware-Version:", "a.espFreq": "ESP32-Takt:", "a.cores": "Kerne @", "a.flashSize": "ESP32-Flashgröße:", "a.mem": "ESP32-Speicher (frei/gesamt):", "a.zbHw": "CC2652P-Hardwareversion:", "a.zbFw": "CC2652P-Firmwareversion:", "a.espFw": "ESP32-Firmware", "a.installGh": "Update von GitHub installieren", "a.coordNote": "Koordinator ist die Standardwahl für ZHA und Zigbee2MQTT. Router nur flashen, um einem anderen Zigbee-Netz beizutreten oder es zu erweitern.", "a.zbFwCard": "Zigbee-Firmware", "a.rev": "Rev.", "a.espFwUpd": "ESP32-Datei-Update", "a.zbFwUpd": "Zigbee-Datei-Update",
"md.warn": "WARNUNG", "md.warnBody": "Das Flashen inoffizieller, falscher oder beschädigter Firmware kann das Gerät beschädigen oder unbrauchbar machen!!!", "md.agree": "Ich stimme zu und trage das Risiko", "md.rstEsp": "ESP32 neu starten", "md.rstBody": "Das Gerät startet neu, die Verbindung wird kurz unterbrochen. Fortfahren?", "md.espFw": "ESP32-Firmware", "md.updateNow": "Jetzt updaten", "md.gitUpd": "ESP32-GIT-UPDATE", "md.gitUpdBody": "Lade das neueste Release von GitHub und flashe es. Dieses Fenster schließt sich automatisch nach dem Neustart.", "md.later": "Später", "md.noRemind": "Nicht mehr erinnern", "md.zbSel": "Zigbee-Firmwaretyp wählen", "md.coord": "Koordinator", "md.router": "Router", "md.latestZb": "Neueste Zigbee-Firmware", "md.devRestart": "GERÄTENEUSTART", "md.waitDev": "Warte auf Gerät … Das Fenster schließt sich automatisch nach dem Neustart.", "md.wifiConn": "WLAN-Verbindung", "md.wifiConnecting": "Verbinde mit Netzwerk … Bitte auf Ergebnis warten.", "md.connErr": "Verbindungsfehler, SSID und Passwort prüfen und erneut versuchen", "md.setSaved": "Einstellungen gespeichert", "md.savedBody": "Neue Parameter gespeichert. Manche brauchen einen Neustart.", "md.later2": "Später manuell neu starten", "md.restartNow": "Jetzt neu starten", "md.noResp": "Keine Antwort vom Gerät – evtl. hat es die IP gewechselt oder der USB-Modus ist aktiv.",
"d.saveErr": "Fehler beim Speichern. Netzwerk prüfen", "d.netErr": "Fehler. Netzwerk prüfen", "d.pageErr": "Seitenladefehler: ", "d.githubStart": "GitHub-Download startet …", "d.prepDev": "Gerät wird vorbereitet …", "d.invalidLink": "Ungültiger Firmware-Link", "d.dlFw": "Firmware wird geladen …", "d.dlFail": "Download fehlgeschlagen: ", "d.evTimeout": "EventSource-Timeout", "d.connFailPoll": "Verbindung fehlgeschlagen, Polling-Modus …", "d.downloaded": "Lade … ", "d.dlComplete": "Download fertig, flashe …", "d.flashing": "Flashe … ", "d.flashDone": "Flashen abgeschlossen!", "d.validating": "Prüfe und flashe …", "d.progress": "Fortschritt: ", "d.updDoneReboot": "Update fertig!<br>Neustart!", "d.updAvail": "Update verfügbar: ", "d.upToDate": "Aktuell", "d.noGh": "GitHub nicht erreichbar. Datei-Upload unten nutzen.", "d.noZbIdx": "Firmware-Index nicht erreichbar. Datei-Upload unten nutzen.", "d.noZbFw": "Keine Zigbee-Firmware im Index.", "d.updAvailShort": "Update verfügbar", "d.allUpToDate": "Alle Varianten aktuell", "d.noFwData": "Keine Firmware-Daten verfügbar.", "d.noFwDataSub": "Bitte Internetverbindung prüfen oder später erneut versuchen.", "d.noFwFor": "Keine Firmware gefunden für '", "d.availTypes": "Verfügbare Firmwaretypen:", "d.fwDetails": "Firmware-Details:", "d.name": "Name:", "d.version": "Version:", "d.type": "Typ:", "d.desc": "Beschreibung:", "d.baud": "Baudrate:", "d.dongle": "Dongle-Typ:", "d.localVer": "Lokale Version: v", "d.ghVer": "GitHub-Version: ", "d.file": "Datei: ", "d.errFetch": "Fehler beim Laden der Versionsinfos.", "d.updateAvailEx": "Update verfügbar!", "d.currentLatest": "Aktuell: v", "d.upToDateEx": "Aktuell", "d.currentVer": "Aktuelle Version: v", "d.router": "Router", "d.coord": "Koordinator", "d.default": "Standard", "d.flash": "Flashe ", "d.protected": "Geschützt · ", "d.unknown": "UNBEKANNT", "d.uploadPct": "Lade hoch: ", "d.uplComplete": "Upload fertig! <br>Prüfe & flashe …", "d.opTimeout": "Zeitüberschreitung. Bitte Gerätestatus prüfen.", "d.showHidePw": "Passwort zeigen / verbergen", "d.showHideKey": "Private-Key zeigen / verbergen", "d.darkMode": "Dunkelmodus", "d.lightMode": "Hellmodus", "d.ipSettings": "IP-Einstellungen", "d.loadingFw": "Lade Firmware-Infos …", "d.loading": "Lade …", "d.goTo": "Zu ", "d.newIp": "Neue IP-Adresse: ", "d.devReboot": "Das Gerät startet neu, damit die Einstellungen wirken.", "d.connectedEx": "Verbunden!", "d.logFilter": "Log-Filter", "md.espUpdAvail": "ESP32-Update verfügbar", "d.newFw": "Neue Firmware (${tag}) auf GitHub gefunden. ${count}× heruntergeladen. Updaten unter Über -> ESP32-Firmware.", "d.latestIs": "Neueste: ", "d.dlCount": "${count}× heruntergeladen", "d.updatesAvail": "${n} Update(s) verfügbar", "d.localRev": "lokale Rev.", "d.noNotes": "(keine Release-Notes)", "d.saving": "Speichere …", "d.openNet": "Offenes Netzwerk", "d.language": "Sprache", "d.toggleDark": "Dunkelmodus umschalten",
"sv.yes": "Ja", "sv.no": "Nein", "sv.on": "Ein", "sv.off": "Aus", "sv.connected": "Verbunden", "sv.notConn": "Nicht verbunden", "sv.active": "Aktiv", "sv.asleep": "Standby", "sv.notCfg": "Nicht konfiguriert", "sv.client": "Client", "sv.ap": "AP", "sv.accessPoint": "Zugangspunkt", "sv.ethernet": "Ethernet", "sv.wifi": "WLAN", "sv.wifiFb": "WLAN (Fallback)", "sv.none": "Keine", "sv.apStarted": "AP gestartet", "sv.notStarted": "Nicht gestartet", "sv.serialPass": "Serieller Passthrough", "sv.usbPass": "USB-Passthrough", "sv.zbNet": "Zigbee-zu-Netzwerk", "sv.zbSer": "Zigbee-zu-Seriell", "sv.unknown": "unbekannt", "sv.embedded": "integriert", "sv.external": "extern"
},
zh: {
"l.st": "状态", "l.ge": "通用", "l.ne": "以太网", "l.wf": "Wi-Fi", "l.zi": "Z2M/ZHA", "l.se": "安全", "l.mq": "MQTT", "l.vp": "WireGuard", "l.to": "工具", "l.ab": "关于", "l.lo": "退出登录",
"c.save": "保存", "c.cancel": "取消", "c.close": "关闭", "c.restart": "重启", "c.details": "详情", "c.checkUpd": "检查更新", "c.checking": "正在检查更新…", "c.selFile": "选择文件…", "c.uploading": "正在上传固件…", "c.uplFail": "固件上传失败！", "c.checkNet": "请检查网络连接后重试", "c.enable": "启用", "c.installed": "已安装：",
"s.opMode": "运行模式", "s.zbConn": "Zigbee 连接", "s.conn": "连接状态：", "s.clientIp": "客户端 IP：", "s.connFor": "已连接时长：", "s.coordStatus": "协调器状态：", "s.coordResets": "协调器自动重启：", "s.eth": "以太网", "s.ethStatus": "连接状态：", "s.dhcp": "DHCP：", "s.ip": "IP 地址：", "s.mask": "子网掩码：", "s.gw": "网关：", "s.speed": "连接速率：", "s.mac": "MAC 地址：", "s.devStatus": "设备状态", "s.opModeRow": "运行模式：", "s.ethernet": "以太网：", "s.uplink": "网络上行：", "s.uptime": "运行时间：", "s.temp": "温度：", "s.wifiEn": "Wi-Fi 已启用：", "s.wifiStatus": "Wi-Fi 状态：", "s.wifiApEn": "Wi-Fi 热点已启用：", "s.wifiApStatus": "Wi-Fi 热点状态：", "s.wifi": "Wi-Fi", "s.mode": "模式：", "s.ssid": "无线名称：", "s.subnet": "子网掩码：", "s.signal": "信号：", "s.vpn": "VPN", "s.init": "已初始化：", "s.remote": "远端地址：",
"g.workMode": "工作模式", "g.netMode": "网络模式", "g.recommended": "推荐", "g.netDesc": "以太网连接，Wi-Fi 自动备用", "g.serMode": "串口模式", "g.serDesc": "通过 USB 直连电脑（串口测试）", "g.keepWeb": "保持网络和 Web 访问", "g.keepHint": "按顺序保持一个通道可用（Wi-Fi、以太网或 Wi-Fi 热点），确保 Web 界面始终可以打开。", "g.rtControl": "实时控制", "g.ledHint": "点击按钮切换 LED 状态（仅本次有效，重启后恢复）", "g.pwrLed": "电源指示灯", "g.wifiLed": "Wi-Fi/串口模式灯", "g.ledSaveHint": "以下开关控制 LED 行为（保存后重启依然有效）", "g.disPwr": "关闭电源灯", "g.disUsb": "关闭 USB 灯", "g.sysVar": "系统参数", "g.hostname": "主机名（设备在网络中显示的名称）", "g.logRefresh": "日志刷新间隔（毫秒）", "g.timezone": "时区",
"e.conn": "连接", "e.dhcp": "启用 DHCP（推荐）", "e.ipSet": "IP 设置", "e.ip": "IP 地址", "e.mask": "子网掩码", "e.gw": "默认网关",
"w.title": "Wi-Fi", "w.yourNet": "我的网络", "w.ipSet": "IP 设置", "w.dhcp": "DHCP", "w.asleep": "Wi-Fi 待机中（当前走以太网）— 使用 Wi-Fi 时在此显示实时 IP。", "w.ip": "IP 地址", "w.mask": "子网掩码", "w.router": "路由器", "w.save": "保存", "w.forget": "忽略此网络", "w.noNet": "尚未配置网络", "w.otherNet": "其他网络", "w.joinOther": "加入其他网络…", "w.ssidPh": "网络名称（SSID）", "w.passPh": "密码", "w.join": "加入", "w.connect": "连接", "w.scanning": "正在扫描…", "w.scanTimeout": "扫描超时，点击重试。", "w.noFound": "未发现 Wi-Fi 网络。", "w.connecting": "正在连接…", "w.connected": "已连接", "w.connFailPw": "Wi-Fi 身份验证失败，请检查密码和安全模式。", "w.noSsid": "未找到网络，请检查 SSID、距离以及网络是否正在广播。", "w.security": "不支持此网络的安全模式，请检查路由器设置。", "w.weakSignal": "Wi-Fi 信号太弱或不稳定，请靠近路由器后重试。", "w.apReject": "接入点拒绝了连接，请检查其设置后重试。", "w.connFail": "无法连接，请检查 SSID、密码和路由器。", "w.connNoStatus": "无法获取连接状态。", "w.saveFail": "保存失败，请检查网络。", "w.saved": "已保存", "w.forgetting": "正在忽略…", "w.forgotten": "已忽略该网络。", "w.forgetFail": "忽略网络失败。", "w.forgetConfirm": "确定忽略此 Wi-Fi 网络吗？", "w.enterSsid": "请输入网络名称。", "w.pw8": "密码至少需要 8 位。", "w.wpa": "WPA2/WPA3", "w.open": "开放网络", "w.noNetsYet": "暂无网络。",
"z.serSet": "串口设置", "z.baud": "波特率", "z.port": "端口", "z.cfgGen": "配置生成器", "z.apAlert": "设备当前处于热点模式（IP 为 192.168.1.1）。连接以太网或 Wi-Fi 后 IP 会变化，需要重新生成配置。", "z.cfgGenLabel": "配置生成器", "z.copy": "复制到剪贴板",
"sec.webSrv": "Web 服务", "sec.disWeb": "连接建立后关闭 Web 服务", "sec.enAuth": "启用 Web 登录验证", "sec.user": "用户名", "sec.pass": "密码", "sec.access": "访问控制", "sec.enWhite": "启用连接 IP 白名单", "sec.allowed": "允许的 IP",
"m.conn": "连接", "m.server": "服务器", "m.port": "端口", "m.user": "用户名", "m.pass": "密码", "m.topicDisc": "主题与发现", "m.topic": "主题", "m.interval": "上报间隔（秒）", "m.haDisc": "启用 Home Assistant 自动发现",
"wg.tunnel": "隧道", "wg.localAddr": "本地地址", "wg.localAddrEx": "例如 10.0.0.2/24", "wg.privKey": "本地私钥", "wg.privKeyPh": "输入 WireGuard 私钥", "wg.privNote": "44 位 Base64 私钥", "wg.endpoint": "对端", "wg.remoteAddr": "对端地址", "wg.remoteNote": "对端的 IP 地址或域名", "wg.remotePub": "对端公钥", "wg.remotePubNote": "44 位 Base64 公钥", "wg.port": "端口", "wg.portNote": "UDP 端口，1-65535",
"t.logs": "日志", "t.esp": "ESP32", "t.zbFrames": "Zigbee ZNP 帧", "t.human": "帧解析显示", "t.clear": "清空控制台", "t.checkConn": "检查 Zigbee 连接", "t.checkVer": "检查 Zigbee 版本", "t.modCtrl": "模块控制", "t.rstZb": "重启 Zigbee", "t.zbRefresh": "Zigbee 刷新模式", "t.routerRe": "路由器重连", "t.rstEsp": "重启 ESP32",
"a.device": "设备", "a.brand": "品牌：", "a.name": "名称：", "a.nameVal": "AVATTO Zigbee 智能网关", "a.fw": "固件：", "a.date": "日期：", "a.sysInfo": "系统信息", "a.zstar": "Zig-Star 型号：", "a.mcu": "主控：", "a.zb": "Zigbee：", "a.eth": "以太网：", "a.ifaces": "接口：", "a.if1": "RJ45 以太网（支持 PoE）", "a.if2": "USB Type-C", "a.if3": "Zigbee 天线接口", "a.espFwVer": "ESP32 固件版本：", "a.espFreq": "ESP32 主频：", "a.cores": "核 @", "a.flashSize": "ESP32 闪存：", "a.mem": "ESP32 内存（可用/总计）：", "a.zbHw": "CC2652P 硬件版本：", "a.zbFw": "CC2652P 固件版本：", "a.espFw": "ESP32 固件", "a.installGh": "从 GitHub 安装更新", "a.coordNote": "ZHA 和 Zigbee2MQTT 默认使用协调器固件。只有加入或扩展其他 Zigbee 网络时才刷路由器固件。", "a.zbFwCard": "Zigbee 固件", "a.rev": "版本", "a.espFwUpd": "ESP32 文件更新", "a.zbFwUpd": "Zigbee 文件更新",
"md.warn": "警告", "md.warnBody": "刷入非官方、错误或损坏的固件可能损坏设备甚至变砖！！！", "md.agree": "我已知晓风险并同意", "md.rstEsp": "重启 ESP32", "md.rstBody": "设备即将重启，连接会中断几秒钟，继续吗？", "md.espFw": "ESP32 固件", "md.updateNow": "立即更新", "md.gitUpd": "ESP32 GIT 更新", "md.gitUpdBody": "正在从 GitHub 下载并刷入最新版本，设备重启后本窗口会自动关闭。", "md.later": "稍后", "md.noRemind": "不再提醒", "md.zbSel": "选择 Zigbee 固件类型", "md.coord": "协调器", "md.router": "路由器", "md.latestZb": "最新 Zigbee 固件", "md.devRestart": "设备重启", "md.waitDev": "正在等待设备…设备重启后本窗口会自动关闭。", "md.wifiConn": "Wi-Fi 网络连接", "md.wifiConnecting": "正在连接网络…请等待结果。", "md.connErr": "连接失败，请检查 SSID 和密码后重试", "md.setSaved": "设置已保存", "md.savedBody": "新参数已保存，部分设置需要重启生效。", "md.later2": "稍后手动重启", "md.restartNow": "立即重启", "md.noResp": "设备无响应，可能是 IP 已变化或当前为 USB 模式。",
"d.saveErr": "保存设置失败，请检查网络", "d.netErr": "出错了，请检查网络", "d.pageErr": "页面加载失败：", "d.githubStart": "GitHub 下载开始…", "d.prepDev": "正在准备设备…", "d.invalidLink": "固件链接无效", "d.dlFw": "正在下载固件…", "d.dlFail": "下载失败：", "d.evTimeout": "事件推送超时", "d.connFailPoll": "连接失败，使用轮询模式…", "d.downloaded": "正在下载…", "d.dlComplete": "下载完成，开始刷机…", "d.flashing": "正在刷机…", "d.flashDone": "刷机完成！", "d.validating": "正在校验并刷机…", "d.progress": "进度：", "d.updDoneReboot": "更新完成！<br>正在重启！", "d.updAvail": "有可用更新：", "d.upToDate": "已是最新", "d.noGh": "无法连接 GitHub，请用下方文件上传。", "d.noZbIdx": "无法获取固件索引，请用下方文件上传。", "d.noZbFw": "索引中没有 Zigbee 固件。", "d.updAvailShort": "有可用更新", "d.allUpToDate": "所有版本均为最新", "d.noFwData": "暂无固件信息。", "d.noFwDataSub": "请检查网络连接，稍后再试。", "d.noFwFor": "未找到固件：'", "d.availTypes": "可用固件类型：", "d.fwDetails": "固件详情：", "d.name": "名称：", "d.version": "版本：", "d.type": "类型：", "d.desc": "描述：", "d.baud": "波特率：", "d.dongle": "适配器类型：", "d.localVer": "本地版本：v", "d.ghVer": "GitHub 版本：", "d.file": "文件：", "d.errFetch": "获取版本信息失败。", "d.updateAvailEx": "有可用更新！", "d.currentLatest": "当前：v", "d.upToDateEx": "已是最新", "d.currentVer": "当前版本：v", "d.router": "路由器", "d.coord": "协调器", "d.default": "默认", "d.flash": "刷入 ", "d.protected": "加密 · ", "d.unknown": "未知", "d.uploadPct": "正在上传：", "d.uplComplete": "上传完成！<br>开始校验并刷机…", "d.opTimeout": "操作超时，请检查设备状态。", "d.showHidePw": "显示 / 隐藏密码", "d.showHideKey": "显示 / 隐藏私钥", "d.darkMode": "深色模式", "d.lightMode": "浅色模式", "d.ipSettings": "IP 设置", "d.loadingFw": "正在加载固件信息…", "d.loading": "正在加载…", "d.goTo": "前往 ", "d.newIp": "新 IP 地址为 ", "d.devReboot": "设备即将重启以应用新设置。", "d.connectedEx": "已连接！", "d.logFilter": "日志过滤", "md.espUpdAvail": "ESP32 有可用更新", "d.newFw": "GitHub 发现新固件（${tag}），已被下载 ${count} 次。请前往“关于 -> ESP32 固件”更新。", "d.latestIs": "最新：", "d.dlCount": "已被下载 ${count} 次", "d.updatesAvail": "${n} 个可用更新", "d.localRev": "本地版本", "d.noNotes": "（暂无更新说明）", "d.saving": "正在保存…", "d.openNet": "开放网络", "d.language": "语言", "d.toggleDark": "切换深色模式",
"sv.yes": "是", "sv.no": "否", "sv.on": "开", "sv.off": "关", "sv.connected": "已连接", "sv.notConn": "未连接", "sv.active": "正常", "sv.asleep": "待机中", "sv.notCfg": "未配置", "sv.client": "客户端", "sv.ap": "热点", "sv.accessPoint": "接入点", "sv.ethernet": "以太网", "sv.wifi": "Wi-Fi", "sv.wifiFb": "Wi-Fi（备用）", "sv.none": "无", "sv.apStarted": "热点已启动", "sv.notStarted": "未启动", "sv.serialPass": "串口直通", "sv.usbPass": "USB 直通", "sv.zbNet": "Zigbee 转网络", "sv.zbSer": "Zigbee 转串口", "sv.unknown": "未知", "sv.embedded": "内置", "sv.external": "外置"
}
};
I18N.en["d.releaseNotes"] = "Release notes:";
I18N.de["d.releaseNotes"] = "Versionshinweise:";
I18N.zh["d.releaseNotes"] = "更新说明：";
I18N.en["d.noZbIdx"] = "Could not reach firmware releases. Use file upload below.";
I18N.de["d.noZbIdx"] = "Firmware-Releases nicht erreichbar. Datei-Upload unten nutzen.";
I18N.zh["d.noZbIdx"] = "无法获取固件发布信息，请使用下方文件上传。";
I18N.en["d.noZbFw"] = "No Zigbee firmware release found.";
I18N.de["d.noZbFw"] = "Keine Zigbee-Firmware gefunden.";
I18N.zh["d.noZbFw"] = "未找到 Zigbee 固件发布版本。";
I18N.en["a.zbRole"] = "CC2652P Firmware Role:";
I18N.de["a.zbRole"] = "CC2652P-Firmwarerolle:";
I18N.zh["a.zbRole"] = "CC2652P 固件角色：";
I18N.en["a.switchRole"] = "Switch Zigbee role";
I18N.de["a.switchRole"] = "Zigbee-Rolle wechseln";
I18N.zh["a.switchRole"] = "切换 Zigbee 角色";
I18N.en["a.roleSwitchNote"] = "Switching roles replaces the current firmware role and can stop the current Zigbee network. Continue only if intentional.";
I18N.de["a.roleSwitchNote"] = "Ein Rollenwechsel ersetzt die aktuelle Firmware-Rolle und kann das aktuelle Zigbee-Netz stoppen. Nur fortfahren, wenn dies beabsichtigt ist.";
I18N.zh["a.roleSwitchNote"] = "切换角色会替换当前固件角色，可能导致当前 Zigbee 网络停止。仅在确认这是有意操作时继续。";
I18N.en["a.coordNote"] = "Coordinator is recommended for ZHA and Zigbee2MQTT. Use Router only to join another Zigbee network.";
I18N.de["a.coordNote"] = "Koordinator wird für ZHA und Zigbee2MQTT empfohlen. Router nur verwenden, um einem anderen Zigbee-Netz beizutreten.";
I18N.zh["a.coordNote"] = "ZHA 和 Zigbee2MQTT 推荐使用协调器固件。只有加入其他 Zigbee 网络时才使用路由器固件。";
I18N.en["a.zbIncludePrerelease"] = "Include pre-releases";
I18N.de["a.zbIncludePrerelease"] = "Pre-Releases einschließen";
I18N.zh["a.zbIncludePrerelease"] = "包括预发布版本";
I18N.en["d.prerelease"] = "pre-release";
I18N.de["d.prerelease"] = "Pre-Release";
I18N.zh["d.prerelease"] = "预发布";
I18N.en["d.roleUnknown"] = "Installed Zigbee role could not be detected. Automatic flashing is disabled.";
I18N.de["d.roleUnknown"] = "Die installierte Zigbee-Rolle konnte nicht erkannt werden. Automatisches Flashen ist deaktiviert.";
I18N.zh["d.roleUnknown"] = "无法检测已安装的 Zigbee 角色，已禁用自动刷写。";
I18N.en["d.roleUnknownShort"] = "role unknown";
I18N.de["d.roleUnknownShort"] = "Rolle unbekannt";
I18N.zh["d.roleUnknownShort"] = "角色未知";
I18N.en["d.roleSwitch"] = "requires switching roles";
I18N.de["d.roleSwitch"] = "Rollenwechsel erforderlich";
I18N.zh["d.roleSwitch"] = "需要切换角色";
I18N.en["d.installedRole"] = "Installed role:";
I18N.de["d.installedRole"] = "Installierte Rolle:";
I18N.zh["d.installedRole"] = "已安装角色：";
I18N.en["d.switchTo"] = "Switch to ";
I18N.de["d.switchTo"] = "Wechseln zu ";
I18N.zh["d.switchTo"] = "切换到 ";
I18N.en["d.fwUpToDate"] = "${role} firmware is up to date.";
I18N.de["d.fwUpToDate"] = "Die ${role}-Firmware ist aktuell.";
I18N.zh["d.fwUpToDate"] = "${role} 固件已是最新版本。";
I18N.en["d.fwUpdateAvailable"] = "${role} firmware update available.";
I18N.de["d.fwUpdateAvailable"] = "Update für ${role}-Firmware verfügbar.";
I18N.zh["d.fwUpdateAvailable"] = "${role} 固件有可用更新。";
I18N.en["d.noCurrentRoleFw"] = "No firmware release is available for the installed role.";
I18N.de["d.noCurrentRoleFw"] = "Für die installierte Rolle ist keine Firmware-Version verfügbar.";
I18N.zh["d.noCurrentRoleFw"] = "没有适用于已安装角色的固件版本。";
I18N.en["md.zbRoleSwitch"] = "SWITCH ZIGBEE ROLE?";
I18N.de["md.zbRoleSwitch"] = "ZIGBEE-ROLLE WECHSELN?";
I18N.zh["md.zbRoleSwitch"] = "切换 Zigbee 角色？";
I18N.en["md.zbRoleSwitchBody"] = "This will replace the installed ${current} firmware with ${target} firmware. The new role cannot perform the old role's function. Continue only if this is intentional.";
I18N.de["md.zbRoleSwitchBody"] = "Die installierte ${current}-Firmware wird durch eine ${target}-Firmware ersetzt. Die neue Rolle kann die alte Funktion nicht ausführen. Nur fortfahren, wenn dies beabsichtigt ist.";
I18N.zh["md.zbRoleSwitchBody"] = "这会将已安装的 ${current} 固件替换为 ${target} 固件。新角色无法执行旧角色的功能。仅在确认这是有意操作时继续。";
I18N.en["md.switchRole"] = "Switch role and flash";
I18N.de["md.switchRole"] = "Rolle wechseln und flashen";
I18N.zh["md.switchRole"] = "切换角色并刷写";
I18N.en["md.zbManualConfirm"] = "Confirm Zigbee firmware flash. The selected file may change the device between coordinator and router roles, which can stop the current Zigbee network from working. Continue?";
I18N.de["md.zbManualConfirm"] = "Zigbee-Firmware-Flashen bestätigen. Die ausgewählte Datei kann die Rolle zwischen Koordinator und Router ändern und dadurch das aktuelle Zigbee-Netz außer Betrieb setzen. Fortfahren?";
I18N.zh["md.zbManualConfirm"] = "确认刷写 Zigbee 固件。所选文件可能会在协调器和路由器角色之间切换，从而导致当前 Zigbee 网络停止工作。继续吗？";
const languages = [
	{ value: "en", text: "English", flag: "🇬🇧" },
	{ value: "zh", text: "中文", flag: "🇨🇳" },
	{ value: "de", text: "Deutsch", flag: "🇩🇪" }
];

class MicroI18n {
	constructor(lang = "en") {
		this.locale = languages.some((l) => l.value === lang) ? lang : "en";
		this.vocabulary = (typeof I18N !== "undefined") ? I18N : { en: {} };
	}
	t(key, vars = {}) {
		let text = (this.vocabulary[this.locale] && this.vocabulary[this.locale][key]) || (this.vocabulary.en && this.vocabulary.en[key]) || key;
		Object.keys(vars).forEach((v) => { text = text.replace(`\${${v}}`, vars[v]); });
		return text;
	}
	setLanguage(lng) { if (languages.some((l) => l.value === lng)) this.locale = lng; }
}

let microI18n;

// Shortcut with English fallback even before initLanguage ran.
function t(key, vars) {
	if (typeof microI18n !== "undefined" && microI18n) return microI18n.t(key, vars || {});
	return (typeof I18N !== "undefined" && I18N.en && I18N.en[key]) || key;
}

// Backend status tokens arrive in English (getRootData); map them to the
// active locale for display. Tone detection in statusPill keeps using the
// English source, so pass that in separately where needed.
function tStatus(val) {
	if (val === undefined || val === null) return val;
	const s = String(val);
	const exact = {
		"Yes": "sv.yes", "No": "sv.no", "On": "sv.on", "Off": "sv.off",
		"Connected": "sv.connected", "Not connected": "sv.notConn",
		"Active": "sv.active", "Asleep": "sv.asleep", "Not configured": "sv.notCfg",
		"Client": "sv.client", "AP": "sv.ap", "Access Point": "sv.accessPoint",
		"Ethernet": "sv.ethernet", "WiFi": "sv.wifi", "WiFi (fallback)": "sv.wifiFb",
		"None": "sv.none", "AP started": "sv.apStarted", "Not started": "sv.notStarted",
		"Serial passthrough": "sv.serialPass", "USB passthrough": "sv.usbPass",
		"Zigbee-to-Network": "sv.zbNet", "Zigbee-to-Serial": "sv.zbSer",
		"Coordinator": "d.coord", "Router": "d.router",
		"unknown": "sv.unknown", "Unknown": "sv.unknown",
		"embedded": "sv.embedded", "external": "sv.external",
		"On (Access point)": "sv.on", "N/A": "sv.unknown", "n/a": "sv.unknown", "-": "sv.unknown"
	};
	if (exact[s] !== undefined) return t(exact[s]);
	let m;
	if ((m = s.match(/^Yes, (\d+) connections?$/))) {
		if (!microI18n || microI18n.locale === "en") return s;
		if (microI18n.locale === "de") return "Ja, " + m[1] + (m[1] === "1" ? " Verbindung" : " Verbindungen");
		return "是，" + m[1] + " 个连接";
	}
	if ((m = s.match(/^Silent (\d+)s$/))) {
		if (!microI18n || microI18n.locale === "en") return s;
		if (microI18n.locale === "de") return "Still seit " + m[1] + "s";
		return "静默 " + m[1] + "s";
	}
	if ((m = s.match(/^(\d+) d (\d\d:\d\d:\d\d)$/))) { // uptime / connected-for
		if (!microI18n || microI18n.locale === "en") return s;
		if (microI18n.locale === "de") return m[1] + " T. " + m[2];
		return m[1] + " 天 " + m[2];
	}
	if (s.indexOf("Connected to ") === 0) {
		const rest = s.slice("Connected to ".length);
		if (!microI18n || microI18n.locale === "en") return s;
		if (microI18n.locale === "de") return "Verbunden mit " + rest;
		return "已连接到 " + rest;
	}
	if (s.indexOf("Connecting") === 0) {
		if (!microI18n || microI18n.locale === "en") return s;
		return t("w.connecting") + s.slice("Connecting".length);
	}
	return s;
}

function initLanguage() {
	const savedLang = localStorage.getItem("selected-lang");
	const browserLang = (navigator.language || navigator.userLanguage || "").substring(0, 2);
	const preferredLang = savedLang || (languages.some((l) => l.value === browserLang) ? browserLang : "en");
	microI18n = new MicroI18n(preferredLang);
	document.documentElement.setAttribute("lang", preferredLang);
	updateLangFlags(preferredLang);
	updateLocalizedContent();
}

function setLanguage(lng) {
	if (!languages.some((l) => l.value === lng)) return;
	localStorage.setItem("selected-lang", lng);
	microI18n.setLanguage(lng);
	document.documentElement.setAttribute("lang", lng);
	updateLangFlags(lng);
	updateLocalizedContent();
	// Dynamically rendered rows are built with t() at creation time - rebuild
	// them from cached data so the switch applies without a rescan/reload.
	// A join in flight keeps its progress row (the poll re-renders on finish).
	try {
		const joining = !!document.querySelector(".wifi-join-status.connecting");
		if (getEl("wifiList") && !joining) renderWifiList();
		if (getEl("wifiYourRow")) renderWifiYourNetwork();
		if (isStatusPage()) refreshStatusData();
		if (getEl("espFwStatus")) renderEspFwStatus();
		if (getEl("zbFwList")) renderZbFwList();
	} catch (e) {}
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
			key = key.trim();
			if (key) {
				if (key.includes("[")) {
					const parts = key.split("[");
					const attr = parts[1].slice(0, -1).trim();
					element.setAttribute(attr, microI18n.t(parts[0].trim()));
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
			const label = microI18n.t(pages[key].titleKey);
			document.title = label + TITLE_SUFFIX;
			const pn = document.getElementById("pagenamePC");
			if (pn) pn.textContent = label;
			break;
		}
	}
}

function updateLocalizedContent() {
	localizeTitle(window.location.pathname);
	localize();
}
