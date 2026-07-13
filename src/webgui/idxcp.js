/*
 * plugin/src/webgui/idxcp.js
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * Minimal glue for the settings page's colour controls. Uses NATIVE Unraid form
 * controls (a plain <input type=color> + a text field) — no custom styling, so the
 * page inherits the active Unraid theme. This script only:
 *   - keeps the swatch and the hex field in sync,
 *   - enables Apply on change (Unraid's own listener ignores <input type=color>),
 *   - restores per-section defaults for idxReset() buttons.
 * Loaded as an external <script src> (an inline <script> after a markdown="1" form
 * is mangled by Unraid's MarkdownExtra pass). The colour glue is vanilla JS; the
 * wallpaper/logo pickers use jQuery + Unraid's jquery.filetree (both global in the
 * webGUI), degrading to a plain path field if unavailable.
 */
(function(){
  "use strict";
  function norm(raw){ var s=String(raw==null?'':raw).trim().replace(/^#+/,'').toLowerCase();
    if(/^[0-9a-f]{3}$/.test(s)) s=s.charAt(0)+s.charAt(0)+s.charAt(1)+s.charAt(1)+s.charAt(2)+s.charAt(2);
    return /^[0-9a-f]{6}$/.test(s)?s:null; }
  function markChanged(el){ var f=el.closest('form'); if(f){ var a=f.querySelectorAll('input[value="Apply"]');
    for(var i=0;i<a.length;i++) a[i].disabled=false; } try{ formHasUnsavedChanges=true; }catch(e){} }
  function setup(cp){
    var chip=cp.querySelector('.idxcp-chip'), text=cp.querySelector('.idxcp-hex');
    if(!chip||!text) return;
    var syncing=false;
    chip.addEventListener('input', function(){                 // picked via the swatch
      var n=norm(chip.value); if(!n) return;
      syncing=true; text.value=n;
      text.dispatchEvent(new Event('change',{bubbles:true})); syncing=false; markChanged(text);
    });
    text.addEventListener('input', function(){                 // typed a hex
      if(syncing) return; var n=norm(text.value); if(n){ chip.value='#'+n; markChanged(text); }
    });
    text.addEventListener('change', function(){                // settle -> normalise
      if(syncing) return; var n=norm(text.value); if(n){ text.value=n; chip.value='#'+n; }
    });
    var init=norm(text.value); if(init){ text.value=init; chip.value='#'+init; }
  }
  window.idxReset=function(cls){
    var hx=document.querySelectorAll('.idxcp-hex.'+cls);
    for(var i=0;i<hx.length;i++){
      var cp=hx[i].closest('.idxcp'), chip=cp&&cp.querySelector('.idxcp-chip'),
          d=(hx[i].getAttribute('data-default')||'').toLowerCase();
      hx[i].value=d; if(chip) chip.value='#'+d;
      hx[i].dispatchEvent(new Event('change',{bubbles:true})); markChanged(hx[i]);
    }
    var sel=document.querySelectorAll('select.'+cls+'[data-default]');
    for(var j=0;j<sel.length;j++){ sel[j].value=sel[j].getAttribute('data-default');
      sel[j].dispatchEvent(new Event('change',{bubbles:true})); markChanged(sel[j]); }
  };
  /* wallpaper / header-logo pickers (.idxpick): point at any image already on the
   * server via Unraid's own file browser (jquery.filetree), then save the path to
   * settings.cfg through update.php WITHOUT a #command. The panel watches the path
   * and hot-swaps the image in place, so the change applies live with no restart /
   * no screen flash. Uses jQuery (global in the webGUI) + the filetree plugin the
   * page includes; degrades to the plain (editable) text field if either is absent. */
  function saveImagePath(pick){
    var key=pick.getAttribute('data-key'),
        path=pick.querySelector('.idxpick-path').value,
        note=pick.querySelector('.idxpick-note');
    note.textContent='saving…';
    var body='csrf_token='+encodeURIComponent(typeof csrf_token!=='undefined'?csrf_token:'')
            +'&'+encodeURIComponent('#file')+'='+encodeURIComponent('ugreen-idx6011-pro/panel/settings.cfg')
            +'&'+encodeURIComponent(key)+'='+encodeURIComponent(path);
    fetch('/update.php',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:body})
      .then(function(r){ note.textContent = r.ok ? (path?'saved — panel updating…':'cleared — panel updating…') : 'save failed';
        if(r.ok && typeof window.idxRefreshPreview==='function') window.idxRefreshPreview(); })   /* re-render the live preview */
      .catch(function(){ note.textContent='save failed'; });
  }
  function openTree(input, pick){
    if(typeof jQuery==='undefined' || !jQuery.fn.fileTree){ input.readOnly=false; input.focus(); return; }
    var $=jQuery, row=$(input).closest('.idxpick-row');
    if(row.next('.fileTree').length){ row.next('.fileTree').remove(); return; }   /* toggle closed */
    row.after("<span class='textarea fileTree idxpick-tree'></span>");
    var ft=row.next('.fileTree');
    ft.fileTree({top:'/mnt',root:'/mnt',filter:['png','jpg','jpeg','gif','webp','bmp'],allowBrowsing:true},
      function(file){ input.value=file; ft.slideUp('fast',function(){ft.remove();}); saveImagePath(pick); },
      function(folder){}
    );
    ft.slideDown('fast');
  }
  function setupImagePickers(){
    var picks=document.querySelectorAll('.idxpick');
    for(var i=0;i<picks.length;i++){ (function(pick){
      var input=pick.querySelector('.idxpick-path'),
          browse=pick.querySelector('.idxpick-browse'),
          clear=pick.querySelector('.idxpick-clear');
      if(browse) browse.addEventListener('click',function(){ openTree(input, pick); });
      if(clear)  clear.addEventListener('click',function(){
        input.value=''; var t=pick.querySelector('.fileTree'); if(t) t.remove(); saveImagePath(pick);
      });
      /* covers the degrade path (a manually typed path); setting .value via the
       * picker doesn't fire 'change', so this never double-saves the normal path. */
      if(input) input.addEventListener('change',function(){ saveImagePath(pick); });
    })(picks[i]); }
  }
  /* shared control CSS so the Lighting tab and the Display > Theme tab render with
   * the identical label|control grid design (idxlayout.js reuses this class too). */
  function injectCss(){
    if(document.getElementById('idxcp-css')) return;
    var s=document.createElement('style'); s.id='idxcp-css';
    s.textContent =
      /* label | control grid; the label column auto-sizes to the widest label so
       * rows line up. Cells stretch to fill their track + row height, and each
       * carries a bottom border, so the two cells form ONE full-width row line. */
      '.idxth-grid{display:grid;grid-template-columns:auto 1fr;column-gap:16px;row-gap:0;width:100%;max-width:680px}'+
      /* one faint grey row line per row. `border:0 !important` overrides Unraid's
       * input[type=..]/select border-bottom (specificity 0,1,1) that would otherwise
       * draw a second, darker line; the arrow (a background-image) is left untouched. */
      '.idxth-grid > label,.idxth-grid > select,.idxth-grid > input,.idxth-grid > .idxcp{border:0 !important;border-bottom:1px solid rgba(128,128,128,.22) !important;padding:9px 0;box-sizing:border-box;justify-self:stretch}'+
      '.idxth-grid > label,.idxth-grid > .idxcp{display:flex;align-items:center}'+
      '.idxth-note{grid-column:1/-1;margin:1px 0 7px;padding:0;opacity:.7;font-size:.85em;line-height:1.4}'+
      /* wallpaper / header-logo pickers: bold label, a read-only path field with a
       * Browse (Unraid file tree) + Clear button, one faint grey row line per block */
      '.idxpick{display:block;padding:11px 0 9px;border-bottom:1px solid rgba(128,128,128,.22)}'+
      '.idxpick > label{display:block;font-weight:600;opacity:.9;margin-bottom:7px}'+
      '.idxpick-desc{display:block;font-weight:400;opacity:.6;font-size:.8em;margin-top:2px}'+
      '.idxpick-row{display:flex;flex-wrap:wrap;gap:8px;align-items:center}'+
      '.idxpick-path{flex:1 1 220px;min-width:0;border:1px solid rgba(128,128,128,.35) !important;border-radius:4px;padding:5px 8px;background:transparent}'+
      '.idxpick-note{display:block;opacity:.7;font-size:.85em;min-height:1.15em;margin-top:6px}'+
      /* contain the Unraid file tree popup so it scrolls inside the block */
      '.idxpick-tree{display:block;max-height:280px;overflow:auto;margin:8px 0 2px}'+
      '.idxth-grid label{opacity:.9;font-weight:600}'+
      /* swatch + hex on ONE line, left-aligned within the (full-width) cell */
      '.idxcp{gap:9px}'+
      '.idxcp-chip{flex:0 0 auto;width:48px;height:26px;padding:0;border:1px solid rgba(128,128,128,.5);border-radius:13px;background:none;cursor:pointer;overflow:hidden}'+
      /* zero the native swatch inset so the colour fills the whole pill */
      '.idxcp-chip::-webkit-color-swatch-wrapper{padding:0}'+
      '.idxcp-chip::-webkit-color-swatch{border:none;border-radius:12px}'+
      '.idxcp-chip::-moz-color-swatch{border:none;border-radius:12px}'+
      '.idxcp-hex{flex:0 0 auto;width:90px;border:0 !important;box-shadow:none;background:transparent}'+
      '.idxth-sec{grid-column:1/-1;border:none;margin:13px 0 3px;padding:0;font-weight:600;opacity:.8;letter-spacing:.4px;text-transform:uppercase;font-size:.85em}';
    document.head.appendChild(s);
  }
  /* Unraid's Apply-enable listener ignores <input type=time> (like type=color), so
   * wire them up ourselves — e.g. the Screen tab's power-save from/until times. */
  function setupTimeInputs(){
    var ts=document.querySelectorAll('input[type=time]');
    for(var i=0;i<ts.length;i++){ (function(t){
      var h=function(){ markChanged(t); };
      t.addEventListener('input', h); t.addEventListener('change', h);
    })(ts[i]); }
  }
  function initAll(){ injectCss(); var cps=document.querySelectorAll('.idxcp'); for(var i=0;i<cps.length;i++) setup(cps[i]); setupImagePickers(); setupTimeInputs(); }
  if(document.readyState==='loading') document.addEventListener('DOMContentLoaded',initAll); else initAll();
})();
