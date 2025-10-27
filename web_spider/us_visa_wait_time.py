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

    # Find the table (simpler approach since page structure changed)
    table = soup.find('table')

    # Check if the table exists
    if not table:
        raise ValueError("Table not found on the page.")

    # Verify the visa type column header (B1/B2 Average wait times is now in column 1)
    header_row = table.find('tr')
    if header_row:
        headers = header_row.find_all('th')
        if len(headers) > 1:
            visa_header = headers[1].text.strip()
            assert 'B1/B2' in visa_header and 'Average wait times' in visa_header, \
                f"Visa type mismatch: found {visa_header}"

    # List of valid cities
    valid_cities_ca = ["Calgary", "Halifax", "Montreal", "Ottawa", "Quebec", "Toronto", "Vancouver"]
    valid_cities_cn = ["Beijing", "Chengdu", "Guangzhou", "Hong Kong", "Shanghai", "Shenyang", "Wuhan"]
    valid_cities    = valid_cities_ca + valid_cities_cn
    # Extract city and wait time data from table
    city_months = []
    for tr in table.find_all('tr'):
        td_elements = tr.find_all('td')
        if len(td_elements) >= 2:  # Ensure there are at least 2 columns
            city = td_elements[0].text.strip()
            if city not in valid_cities:
                continue
            
            # B1/B2 Average wait times is now in column 1
            wait_time_text = td_elements[1].text.strip()
            
            # Skip if NA or empty
            if wait_time_text == "NA" or wait_time_text == "":
                continue
            
            # Parse the wait time (e.g., "5 Months", "< 0.5 month", "12 months")
            wait_time_parts = wait_time_text.split()
            if len(wait_time_parts) < 2:
                continue
            
            # Extract numeric value
            month_str = wait_time_parts[0]
            if month_str.startswith('<'):
                # Handle "< 0.5 month" cases - treat as 0.5
                if len(wait_time_parts) >= 2:
                    month_str = wait_time_parts[1]
            
            try:
                months = float(month_str)
                city_months.append((city, months))
            except ValueError:
                # Skip if we can't parse the number
                continue

    # Sort list by months in descending order
    city_months.sort(key=lambda x: x[1], reverse=True)

    # Print the sorted city and months
    for city, months in city_months:
        if months == int(months):
            month_value = int(months)
            month_label = "month" if month_value == 1 else "months"
            print(f"{city:<15} {month_value} {month_label}")
        else:
            month_label = "month" if months == 1 else "months"
            print(f"{city:<15} {months} {month_label}")


if __name__ == "__main__":
    print_us_visa_wait_time()

