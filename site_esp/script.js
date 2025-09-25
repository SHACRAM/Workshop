const fetchData = async () => {
    try {
        const response = await fetch('./data.json');
        const data = await response.json();

        const div = document.getElementById("news");
        div.innerHTML = ''; 

        data.forEach((item, index) => {
            const article = document.createElement("div");
            article.className = "article";

            const title = document.createElement("h2");
            title.textContent = item.titre;
            article.appendChild(title);

            const content = document.createElement("p");
            content.textContent = item.description;
            article.appendChild(content);

            const date = document.createElement("p");
            date.textContent = `Publié le: ${item.date}`;
            article.appendChild(date);

            div.appendChild(article);
        });

    } catch (error) {
        console.error("Erreur lors de la récupération des données:", error);
    }
}
fetchData();