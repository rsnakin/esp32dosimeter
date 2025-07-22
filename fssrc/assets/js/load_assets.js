function loadOneAsset(asset) {
  return new Promise(resolve => {
    if (!asset.url) {
      setTimeout(() => {
        asset.func?.();
        resolve();
      }, asset.delay || 0);
      return;
    }

    const head = document.head;
    let el;
    if (asset.rel === 'stylesheet' || asset.url.endsWith('.css')) {
      el       = document.createElement('link');
      el.rel   = 'stylesheet';
      el.href  = asset.url;
    } else {
      el       = document.createElement('script');
      el.src   = asset.url;
      el.async = false;
    }

    el.addEventListener('load', () => {
      asset.func?.();
      resolve();
    });
    el.addEventListener('error', () => {
      console.error('Cant\'t load ', asset.url);
      resolve();
    });

    setTimeout(() => head.appendChild(el), asset.delay || 0);
  });
}

function loadAssetsSequentially(assets) {
  assets.reduce(
    (prev, asset) => prev.then(() => loadOneAsset(asset)),
    Promise.resolve()
  );
}

function chng() {
  if (typeof eve === 'undefined' && window.Raphael?.eve) {
    window.eve = window.Raphael.eve;
  }
}
const assets = [
  { url: '/assets/bootstrap/css/bootstrap.min.css', rel: 'stylesheet', delay: 200 },
  { url: '/assets/js/jquery-3.7.1.min.js' },
  { url: '/assets/js/tools.js?v=66' },
  { url: '/assets/bootstrap/js/bootstrap.min.js' },
  { url: '/assets/js/raphael.min.js', func: function(){document.querySelector('.navbar').style.visibility = 'visible';} },
  { url: '/assets/js/justgage.min.js', func: chng },
  { url: '/fsdata.js' },
  { func: startPage, delay: 500 }
];

loadAssetsSequentially(assets);
