<?php
/*
 * plugin/src/webgui/preview.php
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Live-preview endpoint for the Layout editor. Renders ONE dashboard page with a
 * DRAFT layout via `panel_dash --preview` (offscreen, no panel needed) and streams
 * the PNG back. The Theme tab additionally passes draft colour / font / text-size
 * choices as PANEL_* env overrides so they preview live without being saved. All
 * inputs are sanitised; panel_dash also skips any unknown ids.
 */
$page = isset($_GET['page']) ? (int)$_GET['page'] : 0;
if ($page < 0 || $page > 11) $page = 0;   /* up to MAX_CPAGES pages */
$layout = isset($_GET['layout']) ? strtolower($_GET['layout']) : '';
$layout = preg_replace('/[^a-z0-9,:_-]/', '', $layout);

/* draft theme overrides (query param => env var), each sanitised */
$env = '';
$hex = function ($k) {          /* 6-hex colour */
    if (!isset($_GET[$k])) return null;
    $v = preg_replace('/[^0-9a-fA-F]/', '', $_GET[$k]);
    return strlen($v) >= 6 ? substr($v, 0, 6) : null;
};
$colmap = ['accent'=>'PANEL_COL_ACCENT','grada'=>'PANEL_COL_GRAD_A','gradb'=>'PANEL_COL_GRAD_B',
           'bg'=>'PANEL_COL_BG','card'=>'PANEL_COL_CARD','ctext'=>'PANEL_COL_TEXT','dim'=>'PANEL_COL_DIM',
           'ctitle'=>'PANEL_COL_TITLE','ok'=>'PANEL_COL_OK','warn'=>'PANEL_COL_WARN','bad'=>'PANEL_COL_BAD'];
foreach ($colmap as $q => $e) { $c = $hex($q); if ($c !== null) $env .= $e . '=' . escapeshellarg($c) . ' '; }
if (isset($_GET['font'])) {
    $f = preg_replace('/[^A-Za-z0-9]/', '', $_GET['font']);
    if ($f !== '') $env .= 'PANEL_FONT=' . escapeshellarg($f) . ' ';
}
foreach (['head'=>'PANEL_HEAD','text'=>'PANEL_TEXT'] as $q => $e) {
    if (isset($_GET[$q])) { $n = max(70, min(150, (int)$_GET[$q])); $env .= $e . '=' . (int)$n . ' '; }
}
if (isset($_GET['scroll'])) { $s = max(0, min(20000, (int)$_GET['scroll'])); $env .= 'PANEL_SCROLL=' . (int)$s . ' '; }
if (isset($_GET['cardop'])) { $o = max(10, min(100, (int)$_GET['cardop'])); $env .= 'PANEL_CARD_OPACITY=' . (int)$o . ' '; }
if (isset($_GET['bgdim']))  { $d = max(0,  min(80,  (int)$_GET['bgdim']));  $env .= 'PANEL_BG_DIM=' . (int)$d . ' '; }
/* draft per-page chrome toggles (header bar / title card / page dots) */
foreach (['header'=>'PANEL_PAGE_HEADER','title'=>'PANEL_PAGE_TITLE','dots'=>'PANEL_PAGE_DOTS'] as $q => $e) {
    if (isset($_GET[$q])) $env .= $e . '=' . ((int)$_GET[$q] ? 1 : 0) . ' ';
}
/* draft page identity (name + position N/M) so a new/unsaved page previews correctly */
if (isset($_GET['pname'])) { $nm = preg_replace('/[\x00-\x1f]/', '', substr((string)$_GET['pname'], 0, 23)); $env .= 'PANEL_PAGE_NAME=' . escapeshellarg($nm) . ' '; }
if (isset($_GET['ppos']))  $env .= 'PANEL_PAGE_POS='   . max(0, min(99, (int)$_GET['ppos'])) . ' ';
if (isset($_GET['ptot']))  $env .= 'PANEL_PAGE_TOTAL=' . max(1, min(99, (int)$_GET['ptot'])) . ' ';

$bin = '/usr/local/bin/panel_dash';
$tmp = sys_get_temp_dir() . '/idxprev_' . getmypid() . '_' . mt_rand() . '.png';

@exec($env . escapeshellcmd($bin) . ' --preview ' . $page . ' ' .
      escapeshellarg($layout) . ' ' . escapeshellarg($tmp) . ' 2>/dev/null');

if (is_file($tmp) && filesize($tmp) > 0) {
    header('Content-Type: image/png');
    header('Cache-Control: no-store, max-age=0');
    readfile($tmp);
    @unlink($tmp);
} else {
    http_response_code(500);
    header('Content-Type: text/plain');
    echo 'preview failed';
}
