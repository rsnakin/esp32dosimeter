var __VERSION__ = '1.0';
var __BUILD__ = '00239';
function createModalWindow() {
    var modalWindow = '<div class="modal fade" role="dialog" tabindex="-1" id="modal-1">' +
        '<div class="modal-dialog" role="document"><div class="modal-content">' +
        '<div class="modal-header text-white bg-success">' +
        '<h4 class="modal-title" id="modTitle"></h4><button type="button" class="btn-close" ' +
        'data-bs-dismiss="modal" aria-label="Close" id="modalBClose"></button></div>' +
        '<div class="modal-body" style="text-align:center;border-style: none;">' +
        '<p id="modContent"></p></div>' +
        '<div class="modal-footer" style="border-style: none;">' +
        '<button id="modalClose" class="btn btn-light text-bg-success" type="button" ' +
        'data-bs-dismiss="modal">Close</button></div>' +
        '</div></div></div>';
    $('body').append(modalWindow);
    $('#about').on('click', function () {
        const myModal = new bootstrap.Modal(document.getElementById('modal-1'));
        $.get("/api_version", function(data) {
          $('#modTitle').text('About');
          $('#modContent').html('esp32dosimeter WEBApp V' + __VERSION__ + ':' + __BUILD__ +
            '<br>esp32dosimeter ESP32App V' + data.version + ":" + data.build
          );
          myModal.show();
        }).fail(function() {
          $('#modTitle').text('About');
          $('#modContent').html('esp32dosimeter WEBApp V' + __VERSION__ +
            "<br>⚠️ Error getting ESP32App version"
          );
          $('#modalBClose').show();
          $('#modalClose').show();
          myModal.show();
        });
    });
}
function createNavBar() {
    let path = window.location.pathname;
    let target = path.substring(path.lastIndexOf('/') + 1);
    if(target === '') { target = 'index.html'; }
    let mId = target.replace(/\./g, "_");
    $('#' + mId).css({
      color: 'rgba(255,255,255,0.9)'
    });
}
function tsToLocal(ts, omitSeconds = false) {
  const d = new Date(ts * 1000);

  const base =
    d.getFullYear()              + '-' +
    String(d.getMonth() + 1).padStart(2, '0') + '-' +
    String(d.getDate()).padStart(2, '0')      + ' ' +
    String(d.getHours()).padStart(2, '0')     + ':' +
    String(d.getMinutes()).padStart(2, '0');

  return omitSeconds
    ? base
    : base + ':' + String(d.getSeconds()).padStart(2, '0');
}
function formatBytes(bytes, decimals = 3) {
  if (bytes === 0) return '0 B';

  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB', 'PB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));

  const value = bytes / Math.pow(k, i);
  return `${parseFloat(value.toFixed(decimals))} ${sizes[i]}`;
}
function getCurrentDateISO() {
    const today = new Date();
    const year = today.getFullYear();
    const month = String(today.getMonth() + 1).padStart(2, '0'); // месяцы от 0 до 11
    const day = String(today.getDate()).padStart(2, '0');
    return `${year}-${month}-${day}`;
}
function animateColor(element, startColor, endColor, duration = 1000) {
  const start = parseColor(startColor);
  const end = parseColor(endColor);
  const startTime = performance.now();

  function step(currentTime) {
    const progress = Math.min((currentTime - startTime) / duration, 1);
    const r = Math.round(start.r + (end.r - start.r) * progress);
    const g = Math.round(start.g + (end.g - start.g) * progress);
    const b = Math.round(start.b + (end.b - start.b) * progress);
    element.style.color = `rgb(${r}, ${g}, ${b})`;
    if (progress < 1) {
      requestAnimationFrame(step);
    }
  }

  requestAnimationFrame(step);
}
function parseColor(color) {
  const ctx = document.createElement("canvas").getContext("2d");
  ctx.fillStyle = color;
  const computed = ctx.fillStyle;

  if (computed.startsWith("rgb")) {
    const matches = computed.match(/\d+/g);
    if (!matches || matches.length < 3) {
      throw new Error(`Failed to parse color components from: ${color}`);
    }
    return {
      r: Number(matches[0]),
      g: Number(matches[1]),
      b: Number(matches[2])
    };
  } else if (computed.startsWith("#")) {
    let r = parseInt(computed.substr(1, 2), 16);
    let g = parseInt(computed.substr(3, 2), 16);
    let b = parseInt(computed.substr(5, 2), 16);
    return { r, g, b };
  } else {
    throw new Error(`Unknown color format: ${computed}`);
  }
}


