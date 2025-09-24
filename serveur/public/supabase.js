 // Config Supabase
          
 const SUPABASE_URL = "http://127.0.0.1:54321";
        const SUPABASE_ANON_KEY = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZS1kZW1vIiwicm9sZSI6ImFub24iLCJleHAiOjE5ODM4MTI5OTZ9.CRXP1A7WOeoJeXxjNni43kdQwgnWNReilDMblYTn_I0"; // Remplace par ta clé anon
        const supabase = supabase.createClient(SUPABASE_URL, SUPABASE_ANON_KEY);

        const loginForm = document.querySelector(".login-container");
        const emailInput = document.getElementById("email");
        const passwordInput = document.getElementById("password");
        

        loginForm.addEventListener("submit", async (e) => {
            try{
            e.preventDefault();

            const email = emailInput.value;
            const password = passwordInput.value;

            const { data, error } = await supabase.auth.signInWithPassword({ email, password });

            if (error) {
                alert("Erreur de connexion : " + error.message);
            } else {
                alert("Connexion réussie !");
                console.log("Utilisateur :", data.user);
                // Redirection vers une page protégée
                // window.location.href = "dashboard.html";
            }
            } catch (error) {
                console.error("Erreur inattendue :", error);
                alert("Une erreur inattendue est survenue. Veuillez réessayer plus tard.");
            }

        });