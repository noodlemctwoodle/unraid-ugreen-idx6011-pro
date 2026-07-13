<?php
/*
 * plugin/src/webgui/upload.php
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Wallpaper / header-logo upload for the dashboard. POST a file (field "file") with
 * ?kind=wallpaper|logo, or POST ?kind=...&clear=1 to remove it. The image is saved
 * as panel/<kind>.png (re-encoded via GD so PNG/JPG/GIF/WebP all work) and the panel
 * is restarted to apply. Returns JSON {ok:bool,...}.
 */
header('Content-Type: application/json');
$kind = isset($_GET['kind']) ? $_GET['kind'] : '';
if ($kind !== 'wallpaper' && $kind !== 'logo') { echo json_encode(['ok'=>false,'err'=>'bad kind']); exit; }

$dir     = '/boot/config/plugins/ugreen-idx6011-pro/panel';
$dest    = "$dir/$kind.png";
$restart = '/usr/local/emhttp/plugins/ugreen-idx6011-pro/restart.sh';
function apply($restart){ @exec('bash ' . escapeshellarg($restart) . ' >/dev/null 2>&1 &'); }

if (isset($_GET['clear'])) {
    @unlink($dest);
    apply($restart);
    echo json_encode(['ok'=>true,'cleared'=>true]); exit;
}
if (!isset($_FILES['file']) || $_FILES['file']['error'] !== UPLOAD_ERR_OK) {
    echo json_encode(['ok'=>false,'err'=>'no file']); exit;
}
$tmp  = $_FILES['file']['tmp_name'];
$info = @getimagesize($tmp);
if (!$info) { echo json_encode(['ok'=>false,'err'=>'not an image']); exit; }

@mkdir($dir, 0755, true);
$ok = false;
if (function_exists('imagecreatefrompng')) {
    $src = null;
    switch ($info[2]) {
        case IMAGETYPE_PNG:  $src = @imagecreatefrompng($tmp); break;
        case IMAGETYPE_JPEG: $src = @imagecreatefromjpeg($tmp); break;
        case IMAGETYPE_GIF:  $src = @imagecreatefromgif($tmp); break;
        default: if (function_exists('imagecreatefromwebp')) $src = @imagecreatefromwebp($tmp);
    }
    if ($src) { imagesavealpha($src, true); $ok = @imagepng($src, $dest); imagedestroy($src); }
} elseif ($info[2] === IMAGETYPE_PNG) {          /* no GD: accept PNG as-is */
    $ok = @copy($tmp, $dest);
}
if (!$ok) { echo json_encode(['ok'=>false,'err'=>'unsupported image (need PNG)']); exit; }
apply($restart);
echo json_encode(['ok'=>true]);
