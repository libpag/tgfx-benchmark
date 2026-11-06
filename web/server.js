const express = require('express');
const app = express();
const path = require('path');
const {readPackageCatalog} = require('./script/package-catalog');

app.get('/', (req, res) => {
  res.redirect('/benchmark-ui.html');
});

// Enable SharedArrayBuffer
app.use((req, res, next) => {
  res.set('Cross-Origin-Opener-Policy', 'same-origin');
  res.set('Cross-Origin-Embedder-Policy', 'require-corp');
  res.set('Cache-Control', 'no-store, no-cache, must-revalidate');
  res.set('Pragma', 'no-cache');
  res.set('Expires', '0');
  next();
});

app.get('/api/packages', (req, res) => {
  try {
    res.json(readPackageCatalog());
  } catch (error) {
    res.status(500).json({error: error.message});
  }
});

app.use('/packages', express.static(path.join(__dirname, '../../packages')));
app.use('', express.static(path.join(__dirname, '..')));
app.use('', express.static(path.join(__dirname, 'demo')));

app.get('/favicon.ico', (req, res) => {
  res.status(204).end();
});

const port = Number(process.env.PORT || 8061);
app.listen(port, () => {
  console.log(`Server is running on port ${port}`);

  var url = `http://localhost:${port}/benchmark-ui.html`;
  if (process.env.NO_OPEN === '1') {
    console.log(url);
    return;
  }
  var start = (process.platform == 'darwin'? 'open': 'start');
  require('child_process').exec(start + ' ' + url);
});
