/*
 * plugin/src/webgui/idxlayout.js
 *
 * Created by noodlemctwoodle on 13/07/2026.
 *
 * The dashboard LAYOUT EDITOR. A "Theme" tab (colours + font + text sizes, from
 * the server-rendered #idxtheme block) followed by one tab per USER-DEFINED page.
 * Pages are fully dynamic: add / rename / reorder / delete, each with an enable
 * toggle and an editable module list (reorder / add / remove / variant). A LIVE
 * PREVIEW on the right is rendered by the real dashboard via preview.php and
 * reflects the draft theme too. On Apply the editor writes N_PAGES and
 * PAGE<n>_NAME / _LAYOUT / _ON hidden fields; the built-in SETTINGS page is added
 * by the firmware and is not listed here. Catalog + current pages arrive as
 * data-attributes on #idxlayout (an inline <script> would be mangled by markdown).
 * Vanilla JS; layout-only CSS.
 */
(function(){
  "use strict";
  var BASE = '/plugins/ugreen-idx6011-pro/preview.php';
  var MAXP = 12;                        /* must match MAX_CPAGES in prefs.h */
  var COLQ = { COL_ACCENT:'accent', COL_GRAD_A:'grada', COL_GRAD_B:'gradb', COL_BG:'bg',
               COL_CARD:'card', COL_TEXT:'ctext', COL_OK:'ok', COL_WARN:'warn', COL_BAD:'bad' };
  var CAT = [], ITEMS = {}, pages = [], active = 'theme', root = null, form = null, timer = null, themeEl = null;
  var previewScroll = 0, SCROLL_STEP = 220;

  function catMod(id){ for(var i=0;i<CAT.length;i++) if(CAT[i].id===id) return CAT[i]; return null; }
  function parse(s){ var o=[]; (s||'').split(',').forEach(function(t){ t=t.trim(); if(!t)return;
    var i=t.indexOf(':'); o.push({id: i<0?t:t.slice(0,i), variant: i<0?'':t.slice(i+1)}); }); return o; }
  function build(mods){ return mods.map(function(m){ return m.variant ? m.id+':'+m.variant : m.id; }).join(','); }
  function el(tag, cls, txt){ var e=document.createElement(tag); if(cls)e.className=cls; if(txt!=null)e.textContent=txt; return e; }
  function cleanName(s){ return String(s==null?'':s).replace(/[\x00-\x1f]/g,' ').slice(0,23); }

  function markChanged(){ if(!form) return;
    var a=form.querySelectorAll('input[value="Apply"]'); for(var i=0;i<a.length;i++) a[i].disabled=false;
    try{ formHasUnsavedChanges=true; }catch(e){} }

  /* rebuild the hidden N_PAGES / PAGE<n>_* fields from `pages` */
  function syncAll(){ if(!form) return;
    form.querySelectorAll('input.idxl-hidden').forEach(function(e){ e.remove(); });
    function add(name,val){ var e=document.createElement('input'); e.type='hidden'; e.className='idxl-hidden';
      e.name=name; e.value=val; form.appendChild(e); }
    add('N_PAGES', String(pages.length));
    pages.forEach(function(p,i){ add('PAGE'+i+'_NAME', p.name); add('PAGE'+i+'_LAYOUT', build(p.mods)); add('PAGE'+i+'_ON', p.on?'1':'0'); });
  }

  /* draft theme -> preview.php query params (so previews reflect unsaved theme) */
  function themeParams(){
    if(!themeEl) return '';
    var q=[];
    for(var name in COLQ){ var e=themeEl.querySelector('[name="'+name+'"]');
      if(e && e.value) q.push(COLQ[name]+'='+encodeURIComponent(e.value)); }
    ['FONT:font','HEAD_SCALE:head','TEXT_SCALE:text'].forEach(function(pair){
      var kv=pair.split(':'), e=themeEl.querySelector('[name="'+kv[0]+'"]');
      if(e && e.value) q.push(kv[1]+'='+encodeURIComponent(e.value)); });
    return q.join('&');
  }

  function refreshPreview(){
    var img = document.getElementById('idxprev-img'), note = document.getElementById('idxprev-note');
    if(!img) return;
    var pidx, layout;
    if(active==='theme'){ pidx=0; layout = pages.length ? build(pages[0].mods) : ''; }
    else { pidx=active; layout = build(pages[active].mods); }
    var th = themeParams();
    if(note) note.textContent='rendering…';
    clearTimeout(timer);
    timer = setTimeout(function(){
      img.onload = function(){ if(note) note.textContent=''; };
      img.onerror = function(){ if(note) note.textContent='preview failed'; };
      img.src = BASE+'?page='+pidx+'&layout='+encodeURIComponent(layout)+(th?('&'+th):'')+'&scroll='+previewScroll+'&_='+Date.now();
    }, 350);
  }

  function renderTabs(){
    var bar = document.getElementById('idxlayout-tabs'); bar.innerHTML='';
    var th = el('button','idxl-tab'+(active==='theme'?' on':''), 'Theme'); th.type='button';
    th.onclick=function(){ active='theme'; render(); };
    bar.appendChild(th);
    pages.forEach(function(p,i){
      var b = el('button','idxl-tab'+(i===active?' on':''), p.name||('Page '+(i+1))); b.type='button';
      if(!p.on) b.classList.add('off');
      b.onclick=function(){ active=i; render(); };
      bar.appendChild(b);
    });
    if(pages.length < MAXP){
      var add = el('button','idxl-tab idxl-addtab', '+ Add page'); add.type='button';
      add.onclick=function(){ pages.push({ name:'New Page', mods:[], on:true }); active=pages.length-1; changed(); };
      bar.appendChild(add);
    }
  }
  function moduleRow(mods, m, i){
    var cm = catMod(m.id);
    var row = el('div','idxl-row');
    row.appendChild(el('span','idxl-name', (cm||{label:m.id}).label));
    var ctl = el('div','idxl-controls');                 /* selects + reorder buttons sit right after the name */
    if(cm && cm.indexed){                                /* per-item: pick WHICH (name) + how (style) */
      var items = ITEMS[cm.itemsrc] || [];
      var hasStyles = cm.variants && cm.variants.length>1;
      var raw = m.variant||'', ci = raw.indexOf(':');    /* "name" or "name:style" */
      var curName = ci<0 ? raw : raw.slice(0,ci);
      var curStyle = ci<0 ? '' : raw.slice(ci+1);
      if(curName && items.indexOf(curName)<0 && /^\d+$/.test(curName)) curName = items[parseInt(curName,10)]||'';
      if(!curName && items.length) curName = items[0];
      if(hasStyles && !curStyle) curStyle = cm.variants[0];
      var encode = function(){ m.variant = (hasStyles && curStyle && curStyle!==cm.variants[0]) ? (curName+':'+curStyle) : curName; };
      var was = m.variant; encode(); if(m.variant!==was) syncAll();   /* normalise */
      if(items.length){
        var nsel = el('select','idxl-var idxl-item');
        items.forEach(function(nm){ var o=el('option',null,nm); o.value=nm; if(nm===curName) o.selected=true; nsel.appendChild(o); });
        nsel.onchange=function(){ curName=nsel.value; encode(); changed(); };
        ctl.appendChild(nsel);
      } else {                                           /* no live list — fall back to a number */
        var num = el('input','idxl-var'); num.type='number'; num.min='1';
        num.value = String((parseInt(curName,10)||0) + 1);
        num.onchange = function(){ var n=Math.max(1, parseInt(num.value,10)||1);
          num.value=String(n); curName=String(n-1); encode(); changed(); };
        ctl.appendChild(num);
      }
      if(hasStyles){                                     /* visualisation style */
        var ssel = el('select','idxl-var');
        cm.variants.forEach(function(v){ var o=el('option',null,v); o.value=v; if(v===curStyle) o.selected=true; ssel.appendChild(o); });
        ssel.onchange=function(){ curStyle=ssel.value; encode(); changed(); };
        ctl.appendChild(ssel);
      }
    } else if(cm && cm.variants && cm.variants.length>1){
      var sel = el('select','idxl-var');
      cm.variants.forEach(function(v){ var o=el('option',null,v); o.value=v;
        if((m.variant||cm.variants[0])===v) o.selected=true; sel.appendChild(o); });
      sel.onchange=function(){ m.variant = (sel.value===cm.variants[0]?'':sel.value); changed(); };
      ctl.appendChild(sel);
    } else { ctl.appendChild(el('span','idxl-var-none','')); }
    var up=el('button','idxl-btn','▲'); up.type='button'; up.title='Move up'; up.disabled=i===0;
    up.onclick=function(){ mods.splice(i-1,0,mods.splice(i,1)[0]); changed(); };
    var dn=el('button','idxl-btn','▼'); dn.type='button'; dn.title='Move down'; dn.disabled=i===mods.length-1;
    dn.onclick=function(){ mods.splice(i+1,0,mods.splice(i,1)[0]); changed(); };
    var rm=el('button','idxl-btn idxl-rm','×'); rm.type='button'; rm.title='Remove';
    rm.onclick=function(){ mods.splice(i,1); changed(); };
    row.appendChild(ctl);
    var btns = el('div','idxl-buttons');
    btns.appendChild(up); btns.appendChild(dn); btns.appendChild(rm);
    row.appendChild(btns);
    return row;
  }
  function renderEditor(){
    var pane = document.getElementById('idxlayout-pane'); pane.innerHTML='';
    var p = pages[active];

    var hdr = el('div','idxl-pagehdr');
    var nm = el('input','idxl-pagename'); nm.type='text'; nm.value=p.name; nm.maxLength=23; nm.placeholder='Page name';
    nm.oninput=function(){ p.name=cleanName(nm.value); renderTabs(); syncAll(); markChanged(); };
    hdr.appendChild(nm);
    var lf=el('button','idxl-btn','◀'); lf.type='button'; lf.title='Move page left'; lf.disabled=active===0;
    lf.onclick=function(){ pages.splice(active-1,0,pages.splice(active,1)[0]); active--; changed(); };
    var rt=el('button','idxl-btn','▶'); rt.type='button'; rt.title='Move page right'; rt.disabled=active===pages.length-1;
    rt.onclick=function(){ pages.splice(active+1,0,pages.splice(active,1)[0]); active++; changed(); };
    var del=el('button','idxl-btn idxl-rm','Delete'); del.type='button'; del.title='Delete page'; del.disabled=pages.length<=1;
    del.style.width='auto'; del.style.padding='0 8px';
    del.onclick=function(){ if(pages.length<=1) return; pages.splice(active,1); if(active>=pages.length) active=pages.length-1; changed(); };
    hdr.appendChild(lf); hdr.appendChild(rt); hdr.appendChild(del);
    pane.appendChild(hdr);

    var tog = el('label','idxl-toggle'); var cb=el('input'); cb.type='checkbox'; cb.checked=p.on;
    cb.onchange=function(){ p.on=cb.checked; changed(); renderTabs(); };
    tog.appendChild(cb); tog.appendChild(el('span',null,' Show this page on the dashboard'));
    pane.appendChild(tog);

    var list = el('div','idxl-list');
    if(p.mods.length===0) list.appendChild(el('div','idxl-empty','No modules — add one below.'));
    p.mods.forEach(function(m,i){ list.appendChild(moduleRow(p.mods, m, i)); });
    pane.appendChild(list);

    var add = el('div','idxl-add'); var asel=el('select','idxl-addsel');
    var ph=el('option',null,'Add module…'); ph.value=''; asel.appendChild(ph);
    CAT.forEach(function(c){ var o=el('option',null,c.label); o.value=c.id; asel.appendChild(o); });
    asel.onchange=function(){ if(!asel.value) return; p.mods.push({id:asel.value, variant:''}); asel.value=''; changed(); };
    add.appendChild(asel); pane.appendChild(add);
  }
  function render(){
    previewScroll = 0;                                   /* reset preview scroll on view change */
    renderTabs();
    var isTheme = active==='theme';
    var pane = document.getElementById('idxlayout-pane');
    if(pane) pane.style.display = isTheme ? 'none' : '';
    if(themeEl) themeEl.style.display = isTheme ? '' : 'none';
    if(!isTheme) renderEditor();
    refreshPreview();
  }
  function changed(){ syncAll(); markChanged(); render(); }
  function onTheme(){ markChanged(); refreshPreview(); }

  function init(){
    root = document.getElementById('idxlayout'); if(!root) return;
    form = root.closest('form');
    try { CAT = JSON.parse(root.getAttribute('data-catalog') || '[]'); } catch(e){ CAT=[]; }
    try { ITEMS = JSON.parse(root.getAttribute('data-items') || '{}'); } catch(e){ ITEMS={}; }
    var ST = {}; try { ST = JSON.parse(root.getAttribute('data-layouts') || '{}'); } catch(e){}
    (ST.pages || []).forEach(function(p){ pages.push({ name:cleanName(p.name)||'Page', mods:parse(p.layout), on:p.on?true:false }); });
    if(pages.length===0) pages.push({ name:'Overview', mods:parse('host,cpu,mem,net,storage,uptime'), on:true });

    var css = document.createElement('style'); css.textContent =
      '#idxlayout-tabs{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:10px}'+
      '.idxl-tab{padding:5px 12px;border:1px solid #888;border-radius:5px 5px 0 0;background:transparent;cursor:pointer;font:inherit;color:inherit}'+
      '.idxl-tab.on{border-bottom-color:transparent;font-weight:600}'+
      '.idxl-tab.off{opacity:.5;font-style:italic}'+
      '.idxl-addtab{border-style:dashed;opacity:.8}'+
      '#idxlayout-body{display:flex;gap:52px;align-items:flex-start;flex-wrap:wrap}'+
      '#idxlayout-pane{flex:0 1 780px;min-width:290px}'+
      /* hug a fixed control column so the live preview sits right beside it and the
       * wallpaper/header lines trim to the same width (not full-bleed) */
      '#idxtheme{flex:0 1 680px;min-width:290px}'+
      '.idxth-uploads{max-width:680px;margin-top:54px}'+
      /* .idxth-grid styling is shared from idxcp.js (used by the Lighting tab too) */
      '.idxl-pagehdr{display:flex;align-items:center;gap:6px;margin-bottom:12px}'+
      '.idxl-pagename{flex:1;min-width:120px;padding:6px 2px;font:inherit;font-weight:600;border:0 !important;border-bottom:1px solid rgba(128,128,128,.22) !important;background-color:transparent}'+
      '.idxl-toggle{display:flex;align-items:center;gap:8px;margin:2px 0 26px;cursor:pointer}'+
      '.idxl-list{display:flex;flex-direction:column;gap:0;margin-bottom:12px}'+
      /* compact line rows to match the theme tab (no boxes, one faint grey line);
       * the label grows so the (width-capped) style/name selects + buttons sit right */
      /* three aligned columns: name | dropdowns (fill the middle) | reorder buttons */
      '.idxl-row{display:grid;grid-template-columns:minmax(120px,auto) 1fr auto;align-items:center;column-gap:20px;padding:10px 2px;border-bottom:1px solid rgba(128,128,128,.22)}'+
      '.idxl-name{font-weight:600;opacity:.9}'+
      '.idxl-controls{display:flex;align-items:center;gap:14px;min-width:0}'+
      '.idxl-var{flex:1 1 0;min-width:0;width:auto !important;border:0 !important;background-color:transparent}'+
      '.idxl-buttons{display:flex;align-items:center;gap:8px}'+
      '.idxl-var-none{width:76px}'+
      '.idxl-btn{min-width:28px;height:28px;border:1px solid #888;border-radius:5px;background:transparent;color:inherit;cursor:pointer;font:inherit;line-height:1}'+
      '.idxl-btn:disabled{opacity:.3;cursor:default}.idxl-rm{color:#c33}'+
      '.idxl-empty{opacity:.6;font-style:italic;padding:6px 2px}'+
      '.idxl-add{margin-bottom:8px}.idxl-addsel{min-width:180px;border:0 !important;border-bottom:1px solid rgba(128,128,128,.22) !important;background-color:transparent}'+
      '#idxprev-wrap{flex:0 0 auto;text-align:center}'+
      '#idxprev-img{width:170px;height:auto;border:1px solid rgba(128,128,128,.5);border-radius:6px;background:#111;display:block}'+
      '#idxprev-note{font-size:.8em;opacity:.7;height:1.2em;margin-top:4px}'+
      '#idxprev-scroll{display:flex;gap:6px;justify-content:center;margin-top:6px}'+
      '#idxprev-scroll .idxl-btn{flex:1;height:30px}'+
      '.idxl-prevlabel{font-size:.8em;opacity:.6;margin-bottom:5px;letter-spacing:.5px;text-transform:uppercase}';
    document.head.appendChild(css);

    root.innerHTML =
      '<div id="idxlayout-tabs"></div>'+
      '<div id="idxlayout-body"><div id="idxlayout-pane"></div>'+
      '<div id="idxprev-wrap"><div class="idxl-prevlabel">Live preview</div>'+
      '<img id="idxprev-img" alt="preview"><div id="idxprev-note"></div>'+
      '<div id="idxprev-scroll"><button type="button" class="idxl-btn" id="idxprev-up">▲</button>'+
      '<button type="button" class="idxl-btn" id="idxprev-down">▼</button></div></div></div>';
    document.getElementById('idxprev-up').onclick = function(){ previewScroll = Math.max(0, previewScroll - SCROLL_STEP); refreshPreview(); };
    document.getElementById('idxprev-down').onclick = function(){ previewScroll += SCROLL_STEP; refreshPreview(); };

    themeEl = document.getElementById('idxtheme');
    if(themeEl){
      var body = document.getElementById('idxlayout-body');
      body.insertBefore(themeEl, document.getElementById('idxprev-wrap'));
      themeEl.addEventListener('input', onTheme);
      themeEl.addEventListener('change', onTheme);
    }
    syncAll(); render();
  }
  if(document.readyState==='loading') document.addEventListener('DOMContentLoaded', init); else init();
})();
