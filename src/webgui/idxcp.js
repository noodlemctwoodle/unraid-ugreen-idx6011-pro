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
 * is mangled by Unraid's MarkdownExtra pass). Vanilla JS, no jQuery dependency.
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
  function initAll(){ var cps=document.querySelectorAll('.idxcp'); for(var i=0;i<cps.length;i++) setup(cps[i]); }
  if(document.readyState==='loading') document.addEventListener('DOMContentLoaded',initAll); else initAll();
})();
