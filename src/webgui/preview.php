<?php
/*
 * plugin/src/webgui/preview.php
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Live-preview endpoint for the layout editor. Renders ONE dashboard page with a
 * DRAFT layout via `panel_dash --preview` (offscreen, no panel needed) and streams
 * the PNG back. The layout string is sanitised to module-id characters and the
 * page index is clamped; panel_dash also skips any unknown ids.
 */
$page = isset($_GET['page']) ? (int)$_GET['page'] : 0;
if ($page < 0 || $page > 5) $page = 0;
$layout = isset($_GET['layout']) ? strtolower($_GET['layout']) : '';
$layout = preg_replace('/[^a-z0-9,:_-]/', '', $layout);

$bin = '/usr/local/bin/panel_dash';
$tmp = sys_get_temp_dir() . '/idxprev_' . getmypid() . '_' . mt_rand() . '.png';

@exec(escapeshellcmd($bin) . ' --preview ' . $page . ' ' .
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
