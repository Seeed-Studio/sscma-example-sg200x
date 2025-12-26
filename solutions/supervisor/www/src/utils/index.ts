// Signal strength conversion
export function getSignalStrengthLevel(rssi: number) {
  if (rssi >= -60) {
    return 4;
  } else if (rssi >= -70) {
    return 3;
  } else if (rssi >= -80) {
    return 2;
  } else {
    return 1;
  }
}

/**
 * @description Convert URL parameters to JSON format
 * @param url
 * @returns {{}|any}
 */
export function parseUrlParam(url: string): {} | any {
  try {
    const search = url.split("?")[1];
    if (!search) {
      return {};
    }
    return JSON.parse(
      '{"' +
        decodeURIComponent(search)
          .replace(/"/g, '\\"')
          .replace(/&/g, '","')
          .replace(/=/g, '":"')
          .replace(/\+/g, " ") +
        '"}'
    );
  } catch (error) {
    return {};
  }
}

// Is development environment
export const isDev = import.meta.env.MODE === "development";

// Password encryption removed - now sends plain text
// The old AES encryption with static key was security theater
export const encryptPassword = (password: string) => {
  return password;
};

export const Version = "2025-08-15 18:00";
