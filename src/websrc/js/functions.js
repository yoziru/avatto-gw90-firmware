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
	API_PAGE_SYSTOOLS: { num: 6, str: "/sys-tools", title: "System and Tools" },
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
		API_GET_FILELIST: 4,
		API_GET_FILE: 5,
		API_SEND_HEX: 6,
		API_WIFICONNECTSTAT: 7,
		API_CMD: 8,
		API_GET_LOG: 9,
		API_FLASH_ZB: 10
	},
	pages: pages
};

function showToast(show) {
	const toast = document.querySelector(".toast");
	if (toast) {
		if (show) {
			toast.classList.add("show");
			toast.classList.remove("hide");
		} else {
			toast.classList.remove("show");
			toast.classList.add("hide");
		}
	}
}

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
		const dismissToast = e.target.closest("[data-bs-dismiss='toast']");
		if (dismissToast) { showToast(false); return; }
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

	if (!(localStorage.getItem("refresh_tip_got") == 1)) {
		toastConstructor("refreshTip");
	}

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

function isHex(txt) {
	var regex = /[0-9A-Fa-f]{21}/g;
	return regex.test(txt);
}

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

function fillFileTable(files) {
	const icon = "<i class='bi bi-filetype-json'></i>";
	const filelist = getEl("filelist");
	files.forEach((elem) => {
		const row = document.createElement("tr");
		row.innerHTML = "<td>" + icon + "<a href='#config_file' onclick=\"readfile('" + elem.filename + "');\">" + elem.filename + "</a></td><td>" + elem.size + "B</td>";
		filelist.appendChild(row);
	});
}

function sendHex() {
	let hexInput = getEl("sendHex");
	let hex = hexInput.value.toUpperCase();
	let hexSize = hex.split(" ").length;
	fetch(apiLink + api.actions.API_SEND_HEX + "&hex=" + hex + "&size=" + hexSize).then(() => {
		hexInput.value = "";
	});
}

function loadPage(url) {
	window.history.pushState("", document.title, url);
	console.log("[loadPage] url: " + url);
	switch (url) {
		case api.pages.API_PAGE_ROOT.str:
			apiGetPage(api.pages.API_PAGE_ROOT);
			break;
		case api.pages.API_PAGE_GENERAL.str:
			apiGetPage(api.pages.API_PAGE_GENERAL, () => {
				if (!getEl("usbMode").checked) KeepWebDsbl(true);
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
				if (getEl("WIFIssid").value.length > 1) {
					setTimeout(() => toggleCollapse("collapseWifiPass", true), 600);
				}
				fetch(apiLink + api.actions.API_GET_PARAM + "&param=coordMode").then(r => r.text()).then(data => {
					if (parseInt(data) != 1) {
						getQA(".card").forEach(c => c.classList.add("card-disabled"));
						toastConstructor("wifiDisabled");
					}
				});
				if (getEl("dhcpWiFi").checked) WifiDhcpDsbl(true);
				else WifiDhcpDsbl(false);
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
				fetch(apiLink + api.actions.API_GET_FILELIST).then(r => r.json()).then(data => fillFileTable(data.files));
				fetch(apiLink + api.actions.API_GET_PARAM + "&param=refreshLogs").then(r => r.text()).then(data => {
					logRefresh(parseInt(data) >= 1000 ? parseInt(data) : 1000);
				});
			});
			break;
		case api.pages.API_PAGE_ABOUT.str:
			apiGetPage(api.pages.API_PAGE_ABOUT);
			break;
		default:
			apiGetPage(api.pages.API_PAGE_ROOT);
			break;
	}
	const toast = document.querySelector(".toast");
	if (url != api.pages.API_PAGE_WIFI.str && toast && toast.classList.contains("show")) {
		if (getEl("toastBody").textContent.indexOf("Wi-Fi mode") > 0) showToast(false);
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
								elem.textContent = valueToSet;
							}
							if (property === "deviceTemp") {
								const tempValue = parseFloat(valueToSet);
								if (!isNaN(tempValue)) {
									const percentage = Math.min((tempValue / 90) * 100, 100);
									const prgTemp = getEl("prgTemp");
									if (prgTemp) prgTemp.style.width = percentage + "%";
								}
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

function toastConstructor(params) {
	const toastBtns = getEl("toastButtons");
	const toastHeader = getEl("toastHeaderText");
	const toastBody = getEl("toastBody");
	if (toastBtns) toastBtns.innerHTML = "";
	if (toastHeader) toastHeader.textContent = "";
	if (toastBody) toastBody.textContent = "";
	switch (params) {
		case "refreshTip":
			if (toastHeader) toastHeader.textContent = "Tip";
			if (toastBody) toastBody.textContent = "Statuses and other information in this window are updated when the page refreshes.";
			if (toastBtns) toastBtns.appendChild(el("button", { type: "button", "class": "btn btn-outline-primary", text: "Got it!", click: () => { localStorage.setItem("refresh_tip_got", 1); showToast(false); } }));
			break;
		case "wifiDisabled":
			if (toastHeader) toastHeader.textContent = "Info";
			if (toastBody) toastBody.textContent = "Wi-Fi page is inactive because Wi-Fi mode is not activated. Activate Wi-Fi in the 'General' settings";
			if (toastBtns) {
				toastBtns.appendChild(el("button", { type: "button", "class": "btn btn-outline-primary", text: "Go to 'General'", click: () => { window.location = "/general"; showToast(false); } }));
				toastBtns.appendChild(el("button", { type: "button", "class": "btn btn-outline-primary", text: "Close", click: () => showToast(false) }));
			}
			break;
		default:
			break;
	}
	showToast(true);
}

function applyTheme(dark) {
	if (dark) document.documentElement.setAttribute("data-theme", "dark");
	else document.documentElement.removeAttribute("data-theme");
	localStorage.setItem("theme", dark ? "dark" : "light");
	const sidebarToggle = document.getElementById("darkModeToggle");
	if (sidebarToggle) sidebarToggle.checked = dark;
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
	const sidebarToggle = document.getElementById("darkModeToggle");
	if (sidebarToggle) {
		sidebarToggle.checked = isDark;
		sidebarToggle.addEventListener("change", function () {
			applyTheme(this.checked);
		});
	}
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

function modalAddSpiner() {
	const btns = getQ(".modal-footer");
	if (btns) btns.appendChild(el("div", { role: "status", "class": "spinner-border text-primary", append: el("span", { "class": classHide }) }));
}

function modalAddClose() {
	const btns = getQ(".modal-footer");
	if (btns) btns.appendChild(el("button", { type: "button", "class": "btn btn-primary", text: "Close", click: closeModal }));
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
		case "flashZB":
			if (hText) hText.textContent = "Zigbee OTA update";
			if (mBody) mBody.innerHTML = "Fetching firmware information...";
			modalAddSpiner();
			processResponsesZB().then(zbData => {
				if (!zbData || !zbData.fwInfo) {
					if (mBody) { mBody.innerHTML = "Error fetching firmware information<br>Check your network!"; mBody.style.color = "red"; }
					if (mBtns) mBtns.innerHTML = "";
					modalAddClose();
					return;
				}
				let fwData = zbData.fwInfo;
				let curZbVer = zbData.localVersion;
				const cl = "col-sm-12 mb-2 ";
				if (mBody) mBody.innerHTML = "";
				if (mBtns) mBtns.innerHTML = "";
				modalAddClose();
				if (mBody) mBody.appendChild(el("div", { "class": "container", style: "max-width: 400px", append: el("div", { "class": "row", id: "flashZBrow" }) }));
				const row = getEl("flashZBrow");
				if (row) {
					row.appendChild(el("span", { "class": cl, text: "Your current firmware revision: " + curZbVer }));
					row.appendChild(el("hr", { "class": "border border-dark border-top" }));
					let coordinatorFw = null;
					if (Array.isArray(fwData)) {
						coordinatorFw = fwData.find(fw => fw.name && fw.name.toLowerCase().includes("coordinator") && fw.firmwareType && fw.firmwareType.toLowerCase().includes("zigbee"));
					}
					if (!coordinatorFw) {
						if (mBody) { mBody.innerHTML = "Error: No firmware found in JSON"; mBody.style.color = "red"; }
						if (mBtns) mBtns.innerHTML = "";
						modalAddClose();
						return;
					}
					row.appendChild(el("span", { "class": cl, text: "Available Coordinator firmware:" }));
					row.appendChild(el("textarea", { "class": cl + "form-control", text: "Revision: " + coordinatorFw.version + "\nFile: " + coordinatorFw.name + "\nDescription: " + (coordinatorFw.firmwareDesc || "N/A"), rows: 5, disabled: "" }));
					row.appendChild(el("button", { "class": cl + "btn btn-warning", text: "Flash Coordinator " + coordinatorFw.version, click: function () {
						let fwUrl = coordinatorFw.link;
						if (fwUrl.includes("github.com")) fwUrl = fwUrl.replace("github.com", "raw.githubusercontent.com").replace("/blob/", "/");
						fwUrl = encodeURIComponent(fwUrl);
						console.log("Flash URL:", decodeURIComponent(fwUrl));
						ZBfwStartEvents();
						fetch(apiLink + api.actions.API_FLASH_ZB + "&fwurl=" + fwUrl).then(r => r.text()).then(data => console.log("API Response:", data)).catch(err => { console.error("API Error:", err); if (mBody) { mBody.innerHTML = "API request failed: " + err; mBody.style.color = "red"; } });
					}}));
				}
			}).catch(err => {
				if (mBody) { mBody.innerHTML = "Error fetching firmware information<br>Check your network!"; mBody.style.color = "red"; }
				if (mBtns) mBtns.innerHTML = "";
				modalAddClose();
				console.error("Zigbee OTA error:", err);
			});
			break;
		case "flashWarning":
			if (hText) { hText.textContent = "WARNING"; hText.style.color = "red"; }
			if (mBody) { mBody.textContent = "Flashing unofficial, incorrect or corrupted firmware can damage or brick your device!!!"; mBody.style.color = "red"; }
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-success", text: "Close", click: closeModal }));
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-danger", text: "I agree and I am taking the risk", click: () => { closeModal(); localStorage.setItem("flash_warning", 1); const ub = getEl("updButton"); if (ub) ub.removeAttribute("disabled"); } }));
			}
			break;
		case "espFlashGitInfo":
			if (hText) hText.textContent = "Latest ESP32 firmware";
			if (mBody) { mBody.textContent = params.text; mBody.insertAdjacentHTML("beforeend", params.chglog); }
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-primary", text: "Close", click: closeModal }));
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: "Update now", click: () => { closeModal(); localStorage.setItem("update_notify", 0); espFlashGitWait(); } }));
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
				selRow.appendChild(el("button", { type: "button", "class": "col-sm-12 btn btn-secondary", text: "Cancel", click: closeModal }));
			}
			break;
		case "zbVersionInfo":
			if (hText) hText.textContent = "Latest Zigbee Firmware";
			if (mBody) { mBody.innerHTML = params.chglog; const alertDiv = el("div", { "class": "alert alert-info", html: params.text }); mBody.insertBefore(alertDiv, mBody.firstChild); }
			if (latestZbFirmware) {
				const localVer = latestZbFirmware.localVersion || "unknown";
				const remoteVer = latestZbFirmware.version || "unknown";
				if (remoteVer !== "unknown" && localVer !== "unknown") {
					if (remoteVer > localVer) {
						if (mBody) mBody.appendChild(el("div", { "class": "alert alert-success mt-2", html: "<strong>Update Available!</strong><br>Current: v" + localVer + " -> Latest: v" + remoteVer }));
					} else if (remoteVer === localVer) {
						if (mBody) mBody.appendChild(el("div", { "class": "alert alert-secondary mt-2", html: "<strong>Up to date</strong><br>Current version: v" + localVer }));
					}
				}
			}
			if (mBtns) {
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-primary", text: "Close", click: closeModal }));
				mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: "Update Now", click: () => { closeModal(); zbFlashGitWait(); } }));
			}
			break;
		case "espBetaFeedback":
			if (hText) { hText.textContent = "FW beta feedback"; hText.style.color = "blue"; }
			if (mBody) mBody.textContent = params;
			if (mBtns) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-primary", text: "OK", click: () => { localStorage.setItem("beta_feedback", 1); closeModal(); } }));
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
				if (hText) hText.textContent = "SETTINGS SAVED";
				const wifiModeEl = getEl("wifiMode");
				if (wifiModeEl && wifiModeEl.checked) {
					body += "You will be redirected to the Wi-Fi network selection page.";
					if (mBtns) mBtns.appendChild(el("button", { type: "button", "class": "btn btn-success", text: "Select Wi-Fi network", click: () => { closeModal(); loadPage("/wifi"); } }));
				} else {
					body += "Some settings require a reboot.";
					if (mBtns) {
						mBtns.appendChild(el("button", { type: "button", "class": "btn btn-warning", text: "Restart manually later", click: closeModal }));
						mBtns.appendChild(el("button", { type: "button", "class": "btn btn-success", text: "Restart now", click: () => { closeModal(); espReboot(); rebootWait(); } }));
					}
				}
				if (mBody) mBody.textContent = body;
			}
			break;
		case "keepWeb":
			if (hText) hText.textContent = "Keep network & web server";
			if (mBody) mBody.textContent = "This function allows you to leave one of the communication channels active and have access to the web interface. The device itself will select an available channel according to priority: WIFI, ETHERNET, WIFI AP";
			if (mBtns) mBtns.appendChild(el("button", { type: "button", "class": "btn", text: "Ok", click: closeModal }));
			break;
		default:
			break;
	}
	showModal(true);
}

function WifiDhcpDsbl(state) {
	const ids = ["WifiIp", "WifiMask", "WifiGateway"];
	ids.forEach(id => { const e = getEl(id); if (e) e.disabled = state; });
}

function updateProgress(targetValue) {
    const root = document.documentElement;
    const textEl = document.querySelector(".percent-num");
    root.style.setProperty("--percent", targetValue);
    const duration = 2000;
    const startTime = performance.now();
    function animate(currentTime) {
      const elapsed = currentTime - startTime;
      const progress = Math.min(elapsed / duration, 1);
      if (textEl) {
        if (targetValue === 0 || (targetValue > 0 && targetValue < 100)) textEl.innerText = "Scanning";
        else if (targetValue === 100) textEl.innerText = "Done";
      }
      if (progress < 1) requestAnimationFrame(animate);
    }
    requestAnimationFrame(animate);
}

function getWifiList() {
	updateProgress(0);
	const scanBtn = getEl("wifiScanBtn");
	if (scanBtn) scanBtn.disabled = true;
	var timerId = setInterval(() => { updateProgress(50); }, 500);
	toggleCollapse("collapseWifiPass", false);
	const preloader = getEl("wifiScanPreloader");
	const scanButton = getEl("wifiScanButton");
	if (preloader) preloader.classList.remove(classHide);
	if (scanButton) scanButton.classList.add(classHide);
	fetch(apiLink + api.actions.API_STARTWIFISCAN).then(() => {
		const tmrUpdWifi = setInterval(() => {
			fetch(apiLink + api.actions.API_WIFISCANSTATUS).then(r => r.json()).then(data => {
				if (!data.scanDone) return;
				if (!data.wifi) {
					alert("WiFi networks not found");
					clearInterval(timerId);
					clearInterval(tmrUpdWifi);
					if (scanBtn) scanBtn.disabled = false;
				} else {
					if (data.wifi.length > 0) {
						clearInterval(timerId);
						updateProgress(100);
						const wifiTable = getEl("wifiTable");
						if (wifiTable) wifiTable.innerHTML = "";
						data.wifi.forEach((elem) => {
							const row = document.createElement("tr");
							row.className = "ssidSelector";
							row.id = elem.ssid;
							row.innerHTML = "<td>" + elem.ssid + "</td>";
							let encryptType = "";
							switch (elem.secure) {
								case 2: encryptType = "WPA"; break;
								case 3: case 4: encryptType = "WPA2"; break;
								case 5: encryptType = "WEP"; break;
								case 7: encryptType = "OPEN"; break;
								case 8: encryptType = "AUTO"; break;
							}
							row.innerHTML += "<td>" + encryptType + "</td><td>" + elem.channel + "</td><td>" + elem.rssi + "</td>";
							if (wifiTable) wifiTable.appendChild(row);
						});
						if (preloader) preloader.classList.add(classHide);
						clearInterval(tmrUpdWifi);
						if (scanBtn) scanBtn.disabled = false;
						getQA(".ssidSelector").forEach(row => {
							row.addEventListener("click", function (e) {
								const ssidInput = getEl("WIFIssid");
								const passInput = getEl("WIFIpassword");
								if (ssidInput) ssidInput.value = e.currentTarget.id;
								if (passInput) passInput.value = "";
								toggleCollapse("collapseWifiPass", true);
							});
						});
					} else {
						if (preloader) preloader.classList.add(classHide);
						if (scanButton) scanButton.classList.remove(classHide);
						if (scanBtn) scanBtn.disabled = false;
					}
				}
			});
		}, 2000);
	});
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
				if (sidenav) sidenav.classList.remove("sidenav-active");
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

function SeqInputDsbl(state) {
	["webUser", "webPass"].forEach(id => { const e = getEl(id); if (e) e.disabled = state; });
	const div = getEl("div_show1"); if (div) div.style.display = state ? "none" : "";
}

function SeqInputDsblFw(state) {
	const e = getEl("fwIp"); if (e) e.disabled = state;
	const div = getEl("div_show2"); if (div) div.style.display = state ? "none" : "";
}

function readfile(file) {
	const configFile = getEl("config_file");
	if (configFile) configFile.value = "Loading file: " + file;
	fetch(apiLink + api.actions.API_GET_FILE + "&filename=" + file).then(r => r.text()).then(data => {
		const titleEl = getEl("title");
		const filenameEl = getEl("filename");
		if (titleEl) titleEl.textContent = file;
		if (filenameEl) filenameEl.value = file;
		if (configFile) configFile.value = data;
	});
}

function logRefresh(ms) {
	const prgZb = getEl("prg_zb");
	const barZb = getEl("bar_zb");
	if (prgZb && barZb && barZb.style.width && barZb.style.width !== "0%") {
		console.log("Pausing log refresh during firmware update");
		return;
	}
	var logUpd = setInterval(() => {
		fetch(apiLink + api.actions.API_GET_LOG).then(r => r.text()).then(data => {
			const consoleEl = getEl("console");
			if (consoleEl) consoleEl.value = data;
			else clearInterval(logUpd);
		});
	}, ms);
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
		var gitVer = Number(combinedData.jsonData.tag_name.replace(/[^0-9.]/g, "").split(".").join(""));
		var localVer = Number(combinedData.textData.replace(/[^0-9.]/g, "").split(".").join(""));
		var downloadCount = 0;
		for (var i = 0; i < combinedData.jsonData.assets.length; i++) {
			downloadCount += combinedData.jsonData.assets[i].download_count;
		}
		var releaseInfo = "New firmware (" + combinedData.jsonData.tag_name + ") was found on GitHub. It was downloaded " + downloadCount.toLocaleString() + " times. Go -> System and Tools -> Firmware update page to get more info.";
		var betaInfo = "Thanks for testing new version! Don't forget to give feedback ;)";
		if (gitVer > localVer) {
			setTimeout(() => {
				if (!(localStorage.getItem("update_notify") == 1)) modalConstructor("espGitUpdate", releaseInfo);
				console.log(releaseInfo);
			}, 500);
		} else if (gitVer < localVer) {
			if (!(localStorage.getItem("beta_feedback") == 1)) modalConstructor("espBetaFeedback", betaInfo);
			console.log(betaInfo);
		}
	});
}

async function processResponsesZB() {
    try {
        let [fwData, localVerData] = await Promise.all([
            fetchData(zbFwInfoUrl, true),
            fetchData("/api?action=1&param=zbRev", false)
        ]);
        return { fwInfo: fwData, localVersion: localVerData.trim() };
    } catch (error) {
        console.error("Error while getting Zigbee versions:", error);
        throw error;
    }
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
		const text = "Local version v" + latestZbFirmware.localVersion + ". GitHub version " + latestZbFirmware.version + ", File: " + latestZbFirmware.name + ", Description: " + (latestZbFirmware.firmwareDesc || "N/A");
		const chglog = `<p><strong>Firmware Details:</strong></p><ul><li><strong>Name:</strong> ${latestZbFirmware.name}</li><li><strong>Version:</strong> ${latestZbFirmware.version}</li><li><strong>Type:</strong> ${latestZbFirmware.firmwareType || "N/A"}</li><li><strong>Description:</strong> ${latestZbFirmware.firmwareDesc || "N/A"}</li><li><strong>Baud Rate:</strong> ${latestZbFirmware.baudRate || "N/A"}</li><li><strong>Dongle Type:</strong> ${latestZbFirmware.dongleType || "N/A"}</li></ul>`;
		const mBody = getQ(".modal-body");
		if (mBody) {
			mBody.innerHTML = chglog;
			const alertDiv = el("div", { "class": "alert alert-info", html: text });
			mBody.insertBefore(alertDiv, mBody.firstChild);
		}
	} catch (error) {
		console.error("Error while getting Zigbee version info:", error);
		const mBody = getQ(".modal-body");
		if (mBody) mBody.innerHTML = "<div class='alert alert-danger'><strong>Error fetching version information.</strong><br>Please check your network connection.<br><small>" + (error.message || error) + "</small></div>";
	} finally {
		isZbVersionLoading = false;
	}
}


// ========== Lightweight i18n (MicroI18n) ==========
const languages = [
	{ value: "en", text: "English" },
	{ value: "zh", text: "中文" },
	{ value: "de", text: "Deutsch" }
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
	function populateSel(selId) {
		const sel = document.getElementById(selId);
		if (!sel) return;
		sel.innerHTML = "";
		languages.forEach(({ text, value }) => {
			const opt = document.createElement("option");
			opt.value = value;
			opt.textContent = text;
			sel.appendChild(opt);
		});
		sel.value = preferredLang;
		sel.addEventListener("change", function () {
			localStorage.setItem("selected-lang", this.value);
			["langSel", "langSelSide"].forEach(id => {
				const s = document.getElementById(id);
				if (s) s.value = this.value;
			});
			changeLanguage(this.value);
		});
	}
	populateSel("langSel");
	populateSel("langSelSide");
	updateLocalizedContent();
}

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

function changeLanguage(lng) {
	microI18n.setLanguage(lng);
	updateLocalizedContent();
}
