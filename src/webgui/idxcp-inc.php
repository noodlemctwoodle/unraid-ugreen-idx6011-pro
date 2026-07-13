<?php
/*
 * plugin/src/webgui/idxcp-inc.php
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Shared helpers for the UGREEN iDX6011 Pro settings tabs (Screen + Lighting).
 * Required by each tab .page so the colour/select markup and cfg access stay in
 * one place. Controls are NATIVE Unraid form elements (no custom styling); the
 * only asset is idxcp.js (loaded per tab as an external <script src>).
 */

/* parsed settings.cfg (empty array if absent/invalid) */
function idx_cfg() {
    $f = '/boot/config/plugins/ugreen-idx6011-pro/panel/settings.cfg';
    return is_file($f) ? (@parse_ini_file($f) ?: []) : [];
}
function idx_val($cfg, $k, $def = '') { return isset($cfg[$k]) ? $cfg[$k] : $def; }

/* cache-busted webGUI asset URL */
function idx_asset($f) {
    return "/plugins/ugreen-idx6011-pro/$f?v=" .
           (@filemtime("/usr/local/emhttp/plugins/ugreen-idx6011-pro/$f") ?: '1');
}

/* real network interfaces for the "primary interface" picker */
function idx_ifaces() {
    $r = [];
    foreach (glob('/sys/class/net/*') as $p) {
        $n = basename($p);
        if ($n === 'lo' || $n === 'bonding_masters') continue;
        if (preg_match('/^(veth|docker|tunl|sit|vnet|br-|dummy|wg)/', $n)) continue;
        $r[] = $n;
    }
    sort($r);
    return $r;
}

function idx_opts($items, $cur) {
    foreach ($items as $v => $lbl)
        echo '<option value="' . htmlspecialchars($v) . '"' .
             ((string)$cur === (string)$v ? ' selected' : '') . '>' .
             htmlspecialchars($lbl) . '</option>';
}

/* native colour control: <input type=color> swatch + text hex field, synced by
 * idxcp.js. The text field is the submitted value (BARE 6-hex); $cls tags the
 * section (idx-theme / idx-led) for the per-tab Restore-defaults button. */
function idx_colorField($cfg, $name, $default, $cls) {
    $def = strtolower(ltrim($default, '#'));
    $v   = isset($cfg[$name]) ? strtolower(ltrim(trim($cfg[$name]), '#')) : '';
    if (!preg_match('/^[0-9a-f]{6}$/', $v)) $v = $def;
    $n = htmlspecialchars($name);
    echo '<span class="idxcp">'
       . '<input type="color" class="idxcp-chip" value="#'.$v.'" aria-label="'.$n.' '._('colour').'" title="'._('Pick colour').'"> '
       . '<input type="text" class="idxcp-hex '.$cls.'" name="'.$n.'" value="'.$v.'" data-default="'.$def.'"'
       .        ' size="8" maxlength="7" spellcheck="false" autocomplete="off" aria-label="'.$n.' hex">'
       . '</span>';
}
