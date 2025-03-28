function getSolarTimes() {
    const resultDiv = document.getElementById('risesetresult');
    resultDiv.innerHTML = 'Loading...';

    // Step 1: Get geolocation from IP
    fetch('http://ip-api.com/json')
        .then(response => response.json())
        .then(data => {
            const { lat, lon } = data;
            
            // Step 2: Get sunrise/sunset times
            return fetch(`https://api.sunrisesunset.io/json?lat=${lat}&lng=${lon}`);
        })
        .then(response => response.json())
        .then(data => {
            const { results } = data;
            resultDiv.innerHTML = `
                <h2>Results for your location:</h2>
                <p>Sunrise: ${results.sunrise}</p>
                <p>Sunset: ${results.sunset}</p>
                <p>Dawn: ${results.dawn}</p>
                <p>Dusk: ${results.dusk}</p>
                <p>Day Length: ${results.day_length}</p>
                <p>Solar Noon: ${results.solar_noon}</p>
                <p>Timezone: ${results.timezone}</p>
            `;
        })
        .catch(error => {
            console.error('Error:', error);
            resultDiv.innerHTML = 'An error occurred while fetching the data. Please try again.';
        });
}

var globSunrise = '';
var globSunset = '';

function getSunriseSunsetTimes() {
    const resultDiv = document.getElementById('risesetresult');
    if (resultDiv) {
        resultDiv.innerHTML = 'Loading...';
        resultDiv.style.fontSize = '0.5em'; 
    }
    const beforeSunriseLabel = document.querySelector('label[for="beforeSunrise"]');
    const afterSunsetLabel = document.querySelector('label[for="afterSunset"]');
    const beforeSunriseInput = document.getElementById('beforeSunrise');
    const afterSunsetInput = document.getElementById('afterSunset');
    if (beforeSunriseInput) beforeSunriseInput.value = 10;
    if (afterSunsetInput) afterSunsetInput.value = 10;

    // Step 1: Get geolocation from IP
    fetch('http://ip-api.com/json')
        .then(response => response.json())
        .then(data => {
            const { lat, lon } = data;
            
            // Step 2: Get sunrise/sunset times
            return fetch(`https://api.sunrisesunset.io/json?lat=${lat}&lng=${lon}`);
        })
        .then(response => response.json())
        .then(data => {
            const { results } = data;
            globSunrise = results.sunrise;
            globSunset = results.sunset;
            if (resultDiv) {
                resultDiv.innerHTML = `
                    <p>Sunrise: ${results.sunrise}</p>
                    <p>Sunset: ${results.sunset}</p>
                    <p>Dawn: ${results.dawn}</p>
                    <p>Dusk: ${results.dusk}</p>
                    <p>Day Length: ${results.day_length}</p>
                    <p>Solar Noon: ${results.solar_noon}</p>
                    <p>Timezone: ${results.timezone}</p>
                `;
            }
            // Update the labels with actual sunrise and sunset times
            if (beforeSunriseLabel) {
                document.getElementById('sunrise').value = results.sunrise;
                beforeSunriseLabel.textContent = `minutes before sunrise (today: ${results.sunrise}).`;
            }
            if (afterSunsetLabel) {
                document.getElementById('sunset').value = results.sunset;
                afterSunsetLabel.textContent = `minutes after sunset (today: ${results.sunset}).`;
            }
        })
        .catch(error => {
            console.error('Error:', error);
            if (resultDiv) resultDiv.innerHTML = 'An error occurred while fetching the data. Please try again.';
            if (beforeSunriseLabel) beforeSunriseLabel.textContent = '(err) Minutes before sunrise:';
            if (afterSunsetLabel) afterSunsetLabel.textContent = '(err) Minutes after sunset:';        });
}

document.getElementById('openCloseForm').addEventListener('submit', function(event) {
    const minutes = document.getElementById('openCloseMinutes').value;
    if (minutes === '' || isNaN(minutes) || minutes < 0 || minutes > 120) {
        alert('Please enter a valid number of minutes between 0 and 120.');
        event.preventDefault();
    }
});

function toggleCheckbox(element) {
    var xhr = new XMLHttpRequest();
    if(element.checked){ xhr.open("GET", "/config?door=open", true); }
    else { xhr.open("GET", "/config?door=close", true); }
    xhr.send();
  }