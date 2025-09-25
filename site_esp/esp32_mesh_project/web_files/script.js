let lastUpdateTime = '';
let refreshIntervalId = null;
let isInitialLoad = true;

const fetchData = async () => {
    console.log("🔄 [DEBUG] Début de fetchData() - timestamp:", new Date().toLocaleTimeString());
    
    try {
        console.log("📡 [DEBUG] Envoi requête vers /api/news...");
        const response = await fetch('/api/news');
        console.log("📡 [DEBUG] Réponse reçue. Status:", response.status, "Type:", response.headers.get('content-type'));
        
        const data = await response.json();
        console.log("📊 [DEBUG] Données JSON reçues:", JSON.stringify(data, null, 2));
        console.log("📊 [DEBUG] Type de données:", typeof data, "- Est un array:", Array.isArray(data));

        const div = document.getElementById("news");

        // Gérer le format du master ESP32: {"id":42,"t":"Test","c":"OK"}
        let articles = [];
        
        if (Array.isArray(data)) {
            console.log("✅ [DEBUG] Données détectées comme array avec", data.length, "éléments");
            articles = data;
        } else if (data.articles && Array.isArray(data.articles)) {
            console.log("✅ [DEBUG] Données détectées dans data.articles avec", data.articles.length, "éléments");
            articles = data.articles;
        } else if (data.article) {
            console.log("✅ [DEBUG] Données détectées dans data.article (objet unique)");
            articles = [data.article];
        } else if (data.id && (data.t || data.c)) {
            // Format du master ESP32: {"id":42,"t":"Test","c":"OK"}
            console.log("✅ [DEBUG] Format ESP32 Master détecté - ID:", data.id, "Titre:", data.t, "Contenu:", data.c);
            const masterData = {
                id: data.id,
                title: data.t || "Données ESP32",
                content: data.c || "Aucun contenu",
                created_at: new Date().toLocaleString('fr-FR'),
                source: "ESP32 Master"
            };
            articles = [masterData];
        } else if (typeof data === 'object' && data !== null) {
            console.log("✅ [DEBUG] Objet unique détecté, conversion en array");
            articles = [data];
        }
        
        console.log("📝 [DEBUG] Articles traités:", articles.length, "articles", articles);

        // Si aucun article réel, afficher message d'attente
        if (articles.length === 0 || (articles.length === 1 && (articles[0].title === "En attente" || articles[0].t === "En attente"))) {
            console.log("⏳ [DEBUG] Aucun article réel détecté, affichage du message d'attente");
            if (isInitialLoad) {
                div.innerHTML = '<p class="waiting">🔄 Connexion au système ESP32...</p>';
            } else {
                div.innerHTML = '<p class="waiting">En attente de nouvelles données du master ESP32...</p>';
            }
            return;
        }

        // Vérifier si les données ont changé (utiliser l'ID du master comme référence)
        const currentUpdateTime = articles.map(a => a.id || a.created_at || a.timestamp || a.date).join('|');
        console.log("🔍 [DEBUG] Comparaison timestamps - Ancien:", lastUpdateTime, "- Nouveau:", currentUpdateTime);
        
        if (currentUpdateTime !== lastUpdateTime) {
            console.log("🆕 [DEBUG] Nouvelles données détectées, mise à jour de l'affichage");
            lastUpdateTime = currentUpdateTime;
            
            // Effacer le contenu existant
            div.innerHTML = '';

            // Afficher tous les articles
            console.log("📝 [DEBUG] Début du rendu de", articles.length, "articles");
            articles.forEach((item, index) => {
                console.log(`📄 [DEBUG] Rendu article ${index + 1}:`, {
                    id: item.id,
                    title: item.t || item.title || item.titre,
                    content: item.c || item.content || item.description,
                    date: item.created_at || item.date || item.timestamp,
                    source: item.source
                });
                
                const article = document.createElement("div");
                article.className = "article";

                const title = document.createElement("h2");
                title.textContent = `📡 ESP32: ${item.t || item.title || item.titre || 'Données reçues'}`;
                article.appendChild(title);

                const content = document.createElement("p");
                content.textContent = `Status: ${item.c || item.content || item.description || 'Aucun contenu'}`;
                article.appendChild(content);

                const info = document.createElement("div");
                info.className = "article-info";
                info.innerHTML = `
                    <span>🆔 ID: ${item.id || 'N/A'}</span> | 
                    <span>📅 ${item.created_at || item.date || item.timestamp || 'Maintenant'}</span> |
                    <span>📡 ${item.source || 'ESP32 Master'}</span>
                `;
                article.appendChild(info);

                div.appendChild(article);
            });
            
            // Ajouter un indicateur de dernière mise à jour
            const updateIndicator = document.createElement("div");
            updateIndicator.className = "update-indicator";
            updateIndicator.innerHTML = `
                <p>📡 ${articles.length} donnée(s) reçue(s) du master ESP32</p>
                <p>Dernière mise à jour: ${new Date().toLocaleString()}</p>
                <p>Rafraîchissement automatique toutes les 30 secondes</p>
            `;
            div.appendChild(updateIndicator);
            console.log("✅ [DEBUG] Affichage mis à jour avec succès");
        } else {
            console.log("⏭️ [DEBUG] Aucune nouvelle donnée, conservation de l'affichage actuel");
        }

        isInitialLoad = false;

    } catch (error) {
        console.error("❌ [DEBUG] Erreur lors de la récupération des données:", error);
        console.error("❌ [DEBUG] Type d'erreur:", error.name, "- Message:", error.message);
        
        const div = document.getElementById("news");
        if (isInitialLoad) {
            console.log("⚠️ [DEBUG] Erreur lors du chargement initial");
            div.innerHTML = '<p class="error">🔄 Connexion au système ESP32...</p>';
        } else {
            console.log("⚠️ [DEBUG] Erreur réseau temporaire, conservation des données existantes");
            // Ne pas effacer les données existantes en cas d'erreur réseau temporaire
            const errorDiv = div.querySelector('.error-indicator') || document.createElement('div');
            errorDiv.className = 'error-indicator';
            errorDiv.textContent = "Erreur de connexion temporaire - tentative dans 30 secondes...";
            if (!div.querySelector('.error-indicator')) {
                div.appendChild(errorDiv);
            }
            
            // Supprimer le message d'erreur après 10 secondes
            setTimeout(() => {
                if (div.contains(errorDiv)) {
                    div.removeChild(errorDiv);
                }
            }, 10000);
        }
    }
}

// Fonction pour démarrer le rafraîchissement automatique continu
const startContinuousRefresh = () => {
    console.log("🚀 [DEBUG] Démarrage du rafraîchissement automatique continu (30 secondes)");
    console.log("📍 [DEBUG] Page chargée à:", new Date().toLocaleString());
    console.log("🔗 [DEBUG] URL actuelle:", window.location.href);
    
    // Premier chargement immédiat
    console.log("⚡ [DEBUG] Lancement du premier fetchData()...");
    fetchData();
    
    // Puis toutes les 30 secondes indéfiniment
    refreshIntervalId = setInterval(() => {
        console.log("🔄 [DEBUG] Rafraîchissement automatique planifié - " + new Date().toLocaleTimeString());
        fetchData();
    }, 30000); // 30 secondes
    
    console.log("✅ [DEBUG] Interval ID:", refreshIntervalId);
};

// Fonction pour arrêter le rafraîchissement (utile pour debug)
window.stopAutoRefresh = () => {
    if (refreshIntervalId) {
        clearInterval(refreshIntervalId);
        refreshIntervalId = null;
        console.log("🛑 [DEBUG] Rafraîchissement automatique arrêté");
    }
};

// Debug - Exposer la fonction fetchData pour test manuel
window.debugFetchData = fetchData;

console.log("📱 [DEBUG] Script chargé - Démarrage imminent...");

// Démarrer le rafraîchissement automatique au chargement de la page
if (document.getElementById("news")) {
    startContinuousRefresh();
}


(() => {
  const searchForm = document.getElementById('search-form');
  const searchInput = document.getElementById('search-input');
  const errorEl = document.getElementById('search-error');

  const SECRET_PHRASES = [
    'workshop-B3',
    'le loup est dans la bergerie'
  ].map(s => s.normalize('NFD').replace(/[\u0300-\u036f]/g,'').toLowerCase().trim());

  function normalizeStr(s){
    return s.normalize('NFD').replace(/[\u0300-\u036f]/g,'').toLowerCase().trim();
  }

  function renderFakeResults(query){
    errorEl.textContent = '';
    console.log(`Résultats pour "${query}"`);
  }

  function handleSearchSubmit(e){
    e.preventDefault();
    const raw = searchInput.value || '';
    const normalized = normalizeStr(raw);

    if(SECRET_PHRASES.includes(normalized)){
      window.location.href = './login.html';
    } else if(raw.trim() !== ''){
      renderFakeResults(raw);
    } else {
      errorEl.textContent = '';
    }
  }

  if(searchForm) searchForm.addEventListener('submit', handleSearchSubmit);
})();



(() => {
  const DEMO_USER = 'editor';
  const DEMO_PASS = 'correcthorsebatterystaple'; 

  function createEl(tag, attrs={}, children=[]){
    const el = document.createElement(tag);
    for(const k in attrs){
      if(k === 'class') el.className = attrs[k];
      else if(k === 'html') el.innerHTML = attrs[k];
      else el.setAttribute(k, attrs[k]);
    }
    children.forEach(c => el.appendChild(typeof c === 'string' ? document.createTextNode(c) : c));
    return el;
  }

  let modal = null;
  function buildModal(){
    if(modal) return modal;
    modal = createEl('div', { id:'hidden-modal', class:'hidden-modal', style:'display:flex;position:fixed;inset:0;align-items:center;justify-content:center;background:rgba(0,0,0,0.5);z-index:9999;' });
    const panel = createEl('div', { class:'hidden-panel', style:'width:380px;max-width:94%;background:#fff;border-radius:12px;padding:1rem;box-shadow:0 14px 40px rgba(0,0,0,.25)' });
    const title = createEl('h3', {}, ['Espace réservé']);
    const p = createEl('p', { style:'color:#666;margin:0 0 .6rem' }, ['Identifiez-vous pour accéder au contenu privé.']);
    const form = createEl('form', { id:'hidden-auth-form' });
    const inpU = createEl('input', { name:'username', placeholder:"Nom d'utilisateur", autocomplete:'username', style:'width:100%;padding:.6rem;border:1px solid #e6e6e6;border-radius:8px;margin-top:.4rem' });
    const inpP = createEl('input', { name:'password', type:'password', placeholder:'Mot de passe', autocomplete:'current-password', style:'width:100%;padding:.6rem;border:1px solid #e6e6e6;border-radius:8px;margin-top:.4rem' });
    const err = createEl('div', { class:'hidden-error', style:'color:#b31a2e;margin-top:.5rem;height:1.1em' }, ['']);
    const row = createEl('div', { style:'display:flex;gap:.5rem;margin-top:.75rem;justify-content:flex-end' });
    const submit = createEl('button', { type:'submit', style:'padding:.55rem .8rem;border-radius:8px;border:0;background:#004170;color:#fff;cursor:pointer' }, ['Se connecter']);
    const cancel = createEl('button', { type:'button', style:'padding:.55rem .8rem;border-radius:8px;border:0;background:#f3f6fb;cursor:pointer;margin-right:auto' }, ['Annuler']);
    row.appendChild(cancel);
    row.appendChild(submit);
    form.appendChild(inpU);
    form.appendChild(inpP);
    form.appendChild(err);
    form.appendChild(row);
    panel.appendChild(title);
    panel.appendChild(p);
    panel.appendChild(form);
    modal.appendChild(panel);

    cancel.addEventListener('click', hideModal);
    modal.addEventListener('click', (e)=> { if(e.target === modal) hideModal(); });
    document.addEventListener('keydown', (e)=> { if(e.key === 'Escape') hideModal(); });

    form.addEventListener('submit', (ev)=> {
      ev.preventDefault();
      err.textContent = '';
      const u = inpU.value.trim();
      const pass = inpP.value;
      if(!u || !pass){ err.textContent = 'Remplissez tous les champs.'; return; }
      if(u === DEMO_USER && pass === DEMO_PASS){
        hideModal();
        window.location.href = '/hidden/news.html';
        return;
      } else {
        err.textContent = 'Identifiants incorrects.';
      }
    });

    return modal;
  }

  function showModal(hint){
    const m = buildModal();
    if(!document.body.contains(m)) document.body.appendChild(m);
    m.style.display = 'flex';
    if(hint){
      const u = m.querySelector('input[name="username"]');
      if(u) u.value = hint;
    }
    const first = m.querySelector('input[name="username"]');
    if(first) first.focus();
  }

  function hideModal(){
    if(!modal) return;
    modal.style.display = 'none';
  }

  window.addEventListener('requestHiddenModal', (ev) => {
    const hint = ev && ev.detail && ev.detail.hint ? ev.detail.hint : undefined;
    showModal(hint);
  });
  window.HiddenModal = { show: showModal, hide: hideModal };
  const help = document.getElementById('help-trigger');
  if(help) help.addEventListener('click', (e)=> { e.preventDefault(); showModal(); });

  console.info('hidden.js ready — modal injectée à la demande (prototype).');
})();
