#!/usr/bin/env python3


# pip install  beautifulsoup4 requests
import requests
from bs4 import BeautifulSoup

def print_us_visa_wait_time():
    # URL from where the data will be fetched
    url = "https://travel.state.gov/content/travel/en/us-visas/visa-information-resources/global-visa-wait-times.html"

    # Send an HTTP GET request
    response = requests.get(url)
    response.raise_for_status()  # Raises an HTTPError for bad responses

    # Create a Beautiful Soup object
    soup = BeautifulSoup(response.text, 'html.parser')

    # CSS selector to target the main table body
    table_body_selector = "body > div.tsg-rwd-body-frame-row > div:nth-child(5) > div.tsg-rwd-main-copy-frame.dataCSIpage > div > div.tsg-rwd-content-page-parsysxxx.parsys > div:nth-child(3) > div > p > table > tbody"
    table_body = soup.select_one(table_body_selector)

    # Check if the table body exists
    if not table_body:
        raise ValueError("Table body not found with the specified selector.")

    # CSS selector to retrieve the visa type text
    visa_type_selector = "tr:nth-child(1) > th:nth-child(5) > p:nth-child(2)"
    visa_type = table_body.select_one(visa_type_selector)

    # Assert visa type
    assert visa_type.text.strip() == "Visitors (B1/B2)", f"Visa type mismatch: found {visa_type.text.strip()}"

    # List of valid cities
    valid_cities_ca = ["Calgary", "Halifax", "Montreal", "Ottawa", "Quebec", "Toronto", "Vancouver"]
    valid_cities_cn = ["Beijing", "Chengdu", "Guangzhou", "Hong Kong", "Shanghai", "Shenyang", "Wuhan"]
    valid_cities    = valid_cities_ca + valid_cities_cn
    # Extract city and days data from table
    city_days = []
    for tr in table_body.select('tr'):
        td_elements = tr.find_all('td')
        if len(td_elements) >= 5:  # Ensure there are at least 5 columns
            city = td_elements[0].text.strip()
            if city not in valid_cities:
                continue
            days = td_elements[4].text.strip().split()[0]
            if days == "" or not days[0].isdigit():
                continue
            days = int(days)  # Convert days to integer for sorting
            city_days.append((city, days))

    # Sort list by days in descending order
    city_days.sort(key=lambda x: x[1], reverse=True)

    # Print the sorted city and days
    for city, days in city_days:
        print(f"{city:<15} {days} days")


if __name__ == "__main__":
    print_us_visa_wait_time()

